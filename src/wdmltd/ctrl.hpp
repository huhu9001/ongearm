#pragma once

#include"jobs.hpp"
#include"phantom.hpp"

#include<boost/asio.hpp>

#include<cstddef>
#include<cstdint>
#include<algorithm>
#include<deque>
#include<span>
#include<vector>

struct CtrlPanel {
    struct Ctrl {
        union {
            uint16_t key;
            struct {
                int32_t x, y;
                uint16_t key;
            } tap;
            struct {
                int32_t x, y, r;
                uint16_t up, down, left, right;
            } dpad;
            struct {
                std::chrono::milliseconds t;
                int32_t x_from, y_from;
                int32_t x_to, y_to;
                uint16_t key;
            } swipe;
        };
            
        enum:uint8_t {
            PAUSE,
            SONG,
            TAP,
            DPAD,
            SWIPE,
        };
        uint8_t type;
        bool ctrl, alt, shift;
    };
    explicit CtrlPanel(boost::asio::io_context&ctx) noexcept:
        ctx(ctx), pause{false}, song{false}, ctrl{false}, alt{false}, shift{false}, busy{} {}
    CtrlPanel(CtrlPanel const&) = delete;
    CtrlPanel(CtrlPanel&&) = delete;
    size_t assign(std::span<CtrlPanel::Ctrl const>) noexcept;
    void input(uint16_t key, int32_t status) noexcept;
    void poll(Job&) noexcept;

    struct KeyIndex {
        size_t idx;
        uint16_t key;
        enum:uint8_t {
            PAUSE,
            SONG,
            TAP,
            DPAD_UP,
            DPAD_DOWN,
            DPAD_LEFT,
            DPAD_RIGHT,
            SWIPE,
        };
        uint8_t type;
        bool ctrl, alt, shift;
    };
    
    struct TapCtrl {
        int32_t const x, y;
        uint8_t slot;
    };
    struct DPadCtrl {
        int32_t const x, y, r;
        uint8_t slot;
        bool up, down, left, right;
    };
    struct SwipeCtrl {
        static constexpr auto INTERVAL = std::chrono::milliseconds(16);

        struct Handler {
            SwipeCtrl&sw;
            void operator()(boost::system::error_code const&) noexcept;
        };

        boost::asio::steady_timer timer;
        std::deque<std::vector<uint8_t>>&output;
        size_t step;
        std::chrono::milliseconds const t;
        int32_t const x_from, y_from;
        int32_t const x_to, y_to;
        int32_t const dx, dy;
        uint8_t slot;
    };
private:
    boost::asio::io_context&ctx;
    std::vector<KeyIndex> indices;
    std::deque<std::vector<uint8_t>> output;
    bool pause, song;
    bool ctrl, alt, shift;
    
    static constexpr uint8_t SLOT_NUM = 10;
    bool busy[SLOT_NUM];
    std::vector<TapCtrl> taps;
    std::vector<DPadCtrl> dpads;
    std::vector<SwipeCtrl> swipes;
    
    uint8_t get_slot() noexcept {
        for (size_t i = 0; i < SLOT_NUM; ++i) {
            if (!busy[i]) return static_cast<uint8_t>(i);
        }
        return SLOT_NUM;
    }
};
