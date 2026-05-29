#pragma once

#include"events.hpp"

#include<cstdlib>
#include<span>
#include<string>
#include<vector>

namespace ongearm {
    // for macro systems using relative timestamps, e.g. "sleep 0.5s"
    inline std::vector<std::vector<MotionEvent>> synchronize(
        std::span<MotionEvent const>const motions,
        float off_down, /* time offset for MotionEvent::Kind::DOWN */
        float off_move, /* time offset for MotionEvent::Kind::MOVE */
        float off_up, /* time offset for MotionEvent::Kind::UP */
        size_t num_group, /* number of MotionEvents in a vector */
        std::vector<std::string>*const warnings = nullptr) noexcept {
        /* TODO: not yet implemented */ std::abort();
    }
}