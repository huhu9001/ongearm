#pragma once

#include<events.hpp>
#include<boost/asio.hpp>

#include<cstdint>
#include<atomic>
#include<filesystem>
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

struct PlayMacroContext {
    std::atomic_bool abrt, done;
    boost::asio::ip::tcp::iostream*ptsvr;
    std::vector<PhantomInput>const*song;
};

int change_song(
    std::vector<PhantomInput>&song,
    std::filesystem::path const&name,
    MltdSize const&msize,
    std::vector<std::string>*warnings = nullptr) noexcept;

int play_phantom_macro(PlayMacroContext*const c) noexcept;