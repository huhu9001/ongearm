#pragma once

#include<events.hpp>

#include<cstdint>
#include<string_view>
#include<vector>

struct PhantomInput {
    int64_t time;
    uint8_t size;
    char data[10];
    enum {
        CMD_TOUCH_DOWN = 0x00,
        CMD_TOUCH_MOVE = 0x01,
        CMD_TOUCH_UP = 0x02,
        CMD_TOUCH_CANCEL = 0x03,
        CMD_PING = 0x7f,
    };
};

struct MltdSize {
    int32_t x_left_unit, x_right_unit, y_unit;
    int32_t x_left_solo, x_right_solo, y_solo;
    int32_t x_left_2mix, x_right_2mix;
    int32_t slop;
};

int change_song(
    std::vector<PhantomInput>&song,
    std::string_view name,
    MltdSize const&msize,
    std::vector<std::string>*warnings = nullptr) noexcept;