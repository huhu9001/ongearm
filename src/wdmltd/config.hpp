#pragma once

#include"ctrl.hpp"
#include"phantom.hpp"

#include<cstdint>
#include<string>
#include<string_view>

struct Args {
    std::string err;

    std::string_view cmd;
    std::string_view config;
    std::string_view port;
    std::string_view kbd;
    std::string_view ip;
    std::string_view screen;

    std::vector<std::string_view> inputs;

    Args(int const, char const*const*, int skip) noexcept;
};

struct Config {
    std::string err;
    bool msize_ok;

    int daemon_port;
    std::string phantom_ip;
    std::string phantom_port;
    std::string song_dir;
    std::string ptsvr_jar;
    std::vector<CtrlPanel::Ctrl> ctrls;
    MltdSize msize;

    Config(Args const&args) noexcept;
    std::string load(std::string_view name) noexcept;
};