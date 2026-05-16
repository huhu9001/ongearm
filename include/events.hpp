#pragma once

#include<cstddef>
#include<cstdint>

namespace ongearm {
    struct Pos { int32_t x, y; };

    inline constexpr Pos operator+(Pos p1, Pos p2) noexcept {
        return {p1.x + p2.x, p1.y + p2.y};
    }

    struct NoteEvent {
        int64_t time;
        float x;
        enum class Kind: uint8_t {
            NORM, START, HOLD, END,
            FORTH, LEFT, RIGHT,
        } kind;
        enum class Dexterity: uint8_t {
            EITHER, LEFT, RIGHT,
        } dexterity;
    };

    struct MotionEvent {
        int64_t time;
        Pos pos;
        enum class Kind: uint8_t {
            DOWN, MOVE, UP,
        } kind;
        uint8_t dex;
    };
}