#pragma once

#include"ctrl.hpp"
#include"phantom.hpp"

#include<cstdint>
#include<string>

struct Config {
    std::string ip;
    std::string port;
    std::string kbd;
    std::string song;
    std::string song_dir;
    std::string ptsvr_jar;
    std::vector<Ctrls::Ctrl> ctrls;
    MltdSize msize;
    int16_t key_exit;
    int16_t key_play;

    bool good;
    bool msize_ok;

    Config(int, char**) noexcept;
};