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

Args::Args(int const argc, char const*const*const argv, int const skip) noexcept:
    err{}, cmd{argv[0]}, config{}, port{}, kbd{}, ip{}, screen{}, inputs{} {
    for (int i = 1 + skip; i < argc; ++i) {
        std::string_view const arg{argv[i]};
        if (arg == "--config") {
            i += 1;
            if (i >= argc) {
                err = std::format("missing --config argument");
                return;
            }
            std::error_code ec;
            if (std::filesystem::exists(argv[i], ec))
                config = argv[i];
            else {
                if (ec)
                    err = std::format("fserr: {}", ec.message());
                else
                    err = std::format("config file {} does not exist", argv[i]);
                return;
            }
        }
        else if (arg == "--port") {
            i += 1;
            if (i >= argc) {
                err = std::format("missing --port argument");
                return;
            }
            port = argv[i];
        }
        else if (arg == "--ip") {
            i += 1;
            if (i >= argc) {
                err = std::format("missing --ip argument");
                return;
            }
            ip = argv[i];
        }
        else if (arg == "--kbd") {
            i += 1;
            if (i >= argc) {
                err = std::format("missing --kbd argument");
                return;
            }
            kbd = argv[i];
        }
        else if (arg == "--screen") {
            i += 1;
            if (i >= argc) {
                err = std::format("missing --screen argument");
                return;
            }
            screen = argv[i];
        }
        else inputs.push_back(arg);
    }
}

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

typedef decltype((*static_cast<decltype(toml::parse(""))const*>(nullptr))[""]) NodeView;

static bool msize_from_config(MltdSize&msize, NodeView const screen) noexcept {
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

Config::Config(Args const&args) noexcept:
    err{args.err},
    msize_ok(false),
    daemon_port{43212},
    phantom_port{"27183"} {
    if (!err.empty()) return;

    std::filesystem::path pwd;
    {
        std::error_code ec;
        auto const abs_cmd = std::filesystem::absolute(std::filesystem::path{args.cmd}, ec);
        if (ec) {
            err = std::format("fserr: {}", ec.message());
            return;
        }
        if (abs_cmd.has_parent_path())
            pwd = abs_cmd.parent_path();
    }
    song_dir = pwd;

    if (!args.config.empty()) {
        err = load(args.config);
        if (!err.empty()) return;
    }
    else {
        auto const config = pwd / "config.toml";
        std::error_code ec;
        if (std::filesystem::exists(config, ec)) {
            err = load(config.c_str());
            if (!err.empty()) return;
        }
        else {
            if (ec) {
                err = std::format("fserr: {}", ec.message());
                return;
            }
        }
    }

    if (!args.screen.empty()) {
        if (!msize_from_arg(msize, args.screen)) {
            err = std::format("bad --screen argument \"{}\"", args.screen);
            return;
        }
        msize_ok = true;
    }

    if (!args.ip.empty()) phantom_ip = args.ip;
}

std::string Config::load(std::string_view const name) noexcept {
    auto const config = toml::parse_file(name);
    if (config.failed()) {
        return std::format("bad config: {}", config.error().description());
    }

    if (auto const o = config["port"].value<int>())
        daemon_port = *o;
    if (auto const phantom = config["phantom"]; phantom.is_table()) {
        if (auto const o = phantom["ip"].value<std::string_view>())
            phantom_ip = *o;
        if (auto const o = phantom["port"].value<std::string_view>())
            phantom_port = *o;
    }
    if (auto const o = config["song-path"].value<std::string_view>())
        song_dir = *o;
    if (auto o = config["ptsvr-path"].value<std::string_view>())
        ptsvr_jar = *o;

    if (auto const screen = config["screen"]; screen.is_table()) {
        if (msize_from_config(msize, screen))
            msize_ok = true;
    }

    ctrls.clear();
    if (auto const ctrls_config = config["ctrl"]; ctrls_config.is_array_of_tables()) {
        for (auto&c_node : *ctrls_config.as_array()) {
            auto&c = *c_node.as_table();
            auto const type = c["type"].value_or(std::string_view{});
            auto const ctrl = c["ctrl"].value_or(false);
            auto const alt = c["alt"].value_or(false);
            auto const shift = c["shift"].value_or(false);
            if (type == "pause") {
                auto const key = c["key"].value<uint16_t>();
                if (key) ctrls.push_back(CtrlPanel::Ctrl{
                    .key = *key,
                    .type = CtrlPanel::Ctrl::PAUSE,
                    .ctrl = ctrl,
                    .alt = alt,
                    .shift = shift,
                });
            }
            else if (type == "song") {
                auto const key = c["key"].value<uint16_t>();
                if (key) ctrls.push_back(CtrlPanel::Ctrl{
                    .key = *key,
                    .type = CtrlPanel::Ctrl::SONG,
                    .ctrl = ctrl,
                    .alt = alt,
                    .shift = shift,
                });
            }
            else if (type == "dpad") {
                auto const x = c["x"].value<int32_t>();
                auto const y = c["y"].value<int32_t>();
                auto const r = c["r"].value<int32_t>();
                auto const up = c["up"].value_or<uint16_t>(KEY_RESERVED);
                auto const down = c["down"].value_or<uint16_t>(KEY_RESERVED);
                auto const left = c["left"].value_or<uint16_t>(KEY_RESERVED);
                auto const right = c["right"].value_or<uint16_t>(KEY_RESERVED);
                if (x && y && r) {
                    ctrls.push_back(CtrlPanel::Ctrl{
                        .dpad = {
                            .x = *x,
                            .y = *y,
                            .r = *r,
                            .up = up,
                            .down = down,
                            .left = left,
                            .right = right,
                        },
                        .type = CtrlPanel::Ctrl::DPAD,
                        .ctrl = ctrl,
                        .alt = alt,
                        .shift = shift,
                    });
                }
            }
            else if (type == "swipe") {
                auto const key = c["key"].value<uint16_t>();
                auto const x1 = c["x1"].value<int32_t>();
                auto const y1 = c["y1"].value<int32_t>();
                auto const x2 = c["x2"].value<int32_t>();
                auto const y2 = c["y2"].value<int32_t>();
                if (key && x1 && y1 && x2 && y2) {
                    auto const t =
                        c["time"].value_or<std::chrono::milliseconds::rep>(100);
                    ctrls.push_back(CtrlPanel::Ctrl{
                        .swipe = {
                            .t = std::chrono::milliseconds{t},
                            .x_from = *x1,
                            .y_from = *y1,
                            .x_to = *x2,
                            .y_to = *y2,
                            .key = *key,
                        },
                        .type = CtrlPanel::Ctrl::SWIPE,
                        .ctrl = ctrl,
                        .alt = alt,
                        .shift = shift,
                    });
                }
            }
            else {
                auto const key = c["key"].value<uint16_t>();
                auto const x = c["x"].value<int32_t>();
                auto const y = c["y"].value<int32_t>();
                if (key && x && y) {
                    ctrls.push_back(CtrlPanel::Ctrl{
                        .tap = {
                            .x = *x,
                            .y = *y,
                            .key = *key,
                        },
                        .type = CtrlPanel::Ctrl::TAP,
                        .ctrl = ctrl,
                        .alt = alt,
                        .shift = shift,
                    });
                }
            }
        }
    }

    return {};
}
