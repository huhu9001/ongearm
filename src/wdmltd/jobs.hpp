#pragma once

extern "C" {
    #include<linux/input.h>
}

#include<cstdint>
#include<string>
#include<vector>

struct Job {
    enum Kind {
        NONE,
        ERR,
        CMD,
        KEY,
        PAUSE,
        SONG,
        MOTION,
    } kind;
    
    std::string err;
    struct Cmd {
        enum {
            QUIT,
            PAUSE,
            RESUME,
            CONFIG,
            SONG,
        };
        std::vector<uint8_t> data;
        uint8_t kind;
    } cmd;
    input_event ev;
};
