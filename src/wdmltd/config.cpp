#include"config.hpp"

#include<toml.hpp>

extern "C" {
#include<linux/input.h>
}

#include<cstdio>
#include<filesystem>
#include<print>
#include<string_view>

static bool msize_from_arg(
    MltdSize&msize,
    std::string_view arg) noexcept {
    int32_t*v_must[] = {
        &msize.x_left_unit,
        &msize.x_right_unit,
        &msize.y_unit,
        &msize.x_left_solo,
        &msize.x_right_solo,
        &msize.y_solo,
    };
    int32_t*v_opt[] = {
        &msize.x_left_2mix,
        &msize.x_right_2mix,
        &msize.slop,
    };
    for (auto&v : v_must) {
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
    for (auto&v : v_opt) {
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
    std::string_view config_name;
    
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--config") {
            i += 1;
            if (i >= argc) {
                std::print(stderr, "missing --config argument");
                return;
            }
            if (std::filesystem::exists(argv[i])) config_name = argv[i];
            else {
                std::print(stderr, "config file {} does not exist", argv[i]);
                return;
            }
        }
        else if (arg == "--ip") {
            i += 1;
            if (i >= argc) {
                std::print(stderr, "missing --ip argument");
                return;
            }
            ip = argv[i];
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

    auto const config =
        !config_name.empty() ? toml::parse_file(config_name) :
        std::filesystem::exists("config.toml") ? toml::parse_file("config.toml") :
        toml::parse_result{};

    if (ip.empty())
        if (auto ip_or = config["ip"].value<std::string_view>())
            ip = *ip_or;
    port = config["port"].value_or<std::string_view>("27183");
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

    good = true;
}