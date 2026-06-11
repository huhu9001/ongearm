#pragma once

#include<events.hpp>
#include<boost/asio.hpp>

#include<cstdint>
#include<atomic>
#include<filesystem>
#include<string>
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

struct PhantomSong {
    std::vector<PhantomInput> data;
    std::vector<std::string> warnings;
    std::vector<ongearm::NoteEvent> buf_n;
    std::vector<ongearm::MotionEvent> buf_m;
    int change(std::filesystem::path const&, MltdSize const&) noexcept;
    void clear() noexcept {
        data.clear(), warnings.clear(), buf_n.clear(), buf_m.clear();
    }
};

struct PlayMacroContext {
    std::atomic_bool abrt, done;
    boost::asio::ip::tcp::iostream*ptsvr;
    std::vector<PhantomInput>const*song;
};

int play_phantom_macro(PlayMacroContext*const c) noexcept;