#pragma once

#include"events.hpp"

#include<format>

template<>
struct std::formatter<ongearm::Pos, char> {
    template<typename P> static constexpr P::iterator indiff_parse(P&p) {
        auto it = p.begin();
        while (it != p.end() && *it != '}') ++it;
        return it;
    }
    template<typename P> constexpr P::iterator parse(P&p) {
        return indiff_parse(p);
    }
    template<typename F> F::iterator format(ongearm::Pos pos, F&f) const {
        return std::format_to(f.out(), "({},{})",
            pos.x,
            pos.y);
    }
};

template<>
struct std::formatter<ongearm::NoteEvent, char> {
    template<typename P> constexpr P::iterator parse(P&p) {
        return std::formatter<ongearm::Pos, char>::indiff_parse(p);
    }
    template<typename F> F::iterator format(ongearm::NoteEvent ne, F&f) const {
        return std::format_to(f.out(), "{{{},{:.2},K{},D{}}}",
            ne.time,
            ne.x,
            (size_t)ne.kind,
            (size_t)ne.dexterity);
    }
};

template<>
struct std::formatter<ongearm::MotionEvent, char> {
    template<typename P> constexpr P::iterator parse(P&p) {
        return std::formatter<ongearm::Pos, char>::indiff_parse(p);
    }
    template<typename F> F::iterator format(ongearm::MotionEvent me, F&f) const {
        return std::format_to(f.out(), "{{{},{},K{},{}}}",
            me.time,
            me.pos,
            (size_t)me.kind,
            me.dex);
    }
};