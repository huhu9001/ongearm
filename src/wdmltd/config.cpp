#include"config.hpp"

#include<toml.hpp>

extern "C" {
#include<linux/input.h>
}

#include<cstdio>
#include<filesystem>
#include<print>
#include<string_view>
#include<system_error>

static bool msize_from_arg(
    MltdSize&msize,
    std::string_view arg) noexcept {
    int32_t*const v_must[] = {
        &msize.x_left_unit,
        &msize.x_right_unit,
        &msize.y_unit,
        &msize.x_left_solo,
        &msize.x_right_solo,
        &msize.y_solo,
    };
    for (auto const v : v_must) {
        auto const[ptr, ec] = std::from_chars(arg.begin(), arg.end(), *v);
        if (ec != std::errc{}) return false;
        if (ptr == arg.end()) {
            arg = {};
            continue;
        }
        if (*ptr != ',') return false;
        arg = std::string_view(ptr + 1, arg.end());
    }
    msize.x_left_2mix = msize.x_left_unit;
    msize.x_right_2mix = msize.x_right_unit;
    msize.slop = 40;
    int32_t*const v_opt[] = {
        &msize.x_left_2mix,
        &msize.x_right_2mix,
        &msize.slop,
    };
    for (auto const v : v_opt) {
        auto const[ptr, ec] = std::from_chars(arg.begin(), arg.end(), *v);
        if (ec != std::errc{}) {
            if (arg.empty()) break;
            if (arg.front() != ',') return false;
            arg = arg.substr(1);
            continue;
        }
        if (ptr == arg.end()) break;
        if (*ptr != ',') return false;
        arg = std::string_view(ptr + 1, arg.end());
    }
    return true;
}

static bool msize_from_config(
    MltdSize&msize,
    toml::node_view<toml::node const> screen) noexcept {
    if (auto const v = screen["unit"]["x1"].value<int32_t>()) {
        msize.x_left_unit = *v;
        msize.x_left_2mix = screen["2mix"]["x1"].value_or(*v);
    }
    else return false;
    if (auto const v = screen["unit"]["x2"].value<int32_t>()) {
        msize.x_right_unit = *v;
        msize.x_right_2mix = screen["2mix"]["x2"].value_or(*v);
    }
    else return false;
    if (auto const v = screen["unit"]["y"].value<int32_t>())
        msize.y_unit = *v;
    else return false;
    if (auto const v = screen["solo"]["x1"].value<int32_t>())
        msize.x_left_solo = *v;
    else return false;
    if (auto const v = screen["solo"]["x2"].value<int32_t>())
        msize.x_right_solo = *v;
    else return false;
    if (auto const v = screen["solo"]["y"].value<int32_t>())
        msize.y_solo = *v;
    else return false;
    msize.slop = screen["slop"].value_or<int32_t>(40);
    return true;
}

Config::Config(int argc, char**argv) noexcept:good(false), msize_ok(false) {
    std::error_code ec;
    std::string_view config_name;
    
    for (int i = 0; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--config") {
            i += 1;
            if (i >= argc) {
                std::print(stderr, "missing --config argument");
                return;
            }
            if (std::filesystem::exists(argv[i], ec))
                config_name = argv[i];
            else {
                if (ec)
                    std::print(stderr, "fs: {}", ec.message());
                else
                    std::print(stderr, "config file {} does not exist", argv[i]);
                return;
            }
        }
        else if (arg == "--phantom-ip") {
            i += 1;
            if (i >= argc) {
                std::print(stderr, "missing --phantom-ip argument");
                return;
            }
            phantom_ip = argv[i];
        }
        else if (arg == "--kbd") {
            i += 1;
            if (i >= argc) {
                std::print(stderr, "missing --kbd argument");
                return;
            }
            kbd = argv[i];
        }
        else if (arg == "--screen") {
            i += 1;
            if (i >= argc) {
                std::print(stderr, "missing --screen argument");
                return;
            }
            if (!msize_from_arg(msize, argv[i])) {
                std::println(stderr, "bad --screen argument \"{}\"", argv[i]);
                return;
            }
            msize_ok = true;
        }
        else {
            if (!song.empty())
                std::print(stderr, "song {} ignored. One song only", song);
            song = argv[i];
        }
    }

    if (auto const err = load(config_name); err != 0)
        return;

    good = true;
}

int Config::load(std::string_view const name) noexcept {
    std::error_code ec;
    auto const config =
        !name.empty() ? toml::parse_file(name) :
        std::filesystem::exists("config.toml", ec) ? toml::parse_file("config.toml") :
        toml::parse("");
    if (ec) {
        std::println(stderr, "fs: {}", ec.message());
        return ec.value();
    }
    if (config.failed()) {
        std::println(stderr, "bad config file: {}", config.error().description());
        return -1;
    }

    daemon_port = config["port"].value_or(43212);
    if (auto const phantom = config["phantom"]; phantom.is_table()) {
        if (phantom_ip.empty())
            if (auto const ip_or = phantom["ip"].value<std::string_view>())
                phantom_ip = *ip_or;
        phantom_port = phantom["port"].value_or<std::string_view>("27183");
    }
    else {
        phantom_port = "27183";
    }
    key_exit = config["key-exit"].value_or<uint16_t>(KEY_PAUSE);
    key_play = config["key-play"].value_or<uint16_t>(KEY_Z);
    song_dir = config["song-path"].value_or<std::string_view>(".");
    if (auto jar = config["ptsvr-path"].value<std::string_view>())
        ptsvr_jar = *jar;

    if (!msize_ok) {
        if (auto const screen = config["screen"]; screen.is_table()) {
            if (msize_from_config(msize, screen))
                msize_ok = true;
        }
    }

    if (auto const ctrls_config = config["ctrl"]; ctrls_config.is_array_of_tables()) {
        for (auto&c_node : *ctrls_config.as_array()) {
            auto&c = *c_node.as_table();
            auto const key = c["key"].value<uint16_t>();
            auto const x = c["x"].value<int32_t>();
            auto const y = c["y"].value<int32_t>();
            if (key && x && y)
                ctrls.emplace_back(*key, *x, *y);
        }
    }

    return 0;
}
