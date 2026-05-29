#pragma once

#include"events.hpp"

#include<cmath>
#include<algorithm>
#include<array>
#include<charconv>
#include<format>
#include<istream>
#include<type_traits>
#include<span>
#include<string_view>
#include<vector>

namespace ongearm {
    struct ScrnMapper {
        int32_t left, right, y;
        int32_t touch_slop; /* heuristically, 5% of screen height */
        enum class Orient: uint8_t {
            R0, R90, R180, R270,
        } orient;
        uint8_t num;

        constexpr Pos map(float x) const noexcept {
            int32_t ox;
            auto const fnum = static_cast<float>(num);
            if (x <= 1.0f) ox = left;
            else if (x >= fnum) ox = right;
            else {
                auto const fleft = static_cast<float>(left);
                auto const fright = static_cast<float>(right);
                ox = static_cast<int32_t>(std::lerp(fleft, fright, (x - 1.0f) / (fnum - 1.0f)));
            }

            return orient == Orient::R90 || orient == Orient::R270 ? Pos{y, ox} : Pos{ox, y};
        }

        constexpr float remap(Pos pos) const noexcept {
            auto const fleft = static_cast<float>(left);
            auto const fright = static_cast<float>(right);
            auto const fnum = static_cast<float>(num);
            auto const fx =
                static_cast<float>(orient == Orient::R90 || orient == Orient::R270 ? pos.y : pos.x);
            auto const x = std::lerp(1.0f, fnum, (fx - fleft) / (fright - fleft));
            if (x < 1.0f) return 1.0f;
            if (x > fnum) return fnum;
            return x;
        }

        constexpr Pos offset(int32_t x, int32_t y) const noexcept {
            switch (orient) {
            default:
            case Orient::R0:
                return {x, y};
            case Orient::R90:
                return {y, -x};
            case Orient::R180:
                return {-x, -y};
            case Orient::R270:
                return {-y, x};
            }
        }
    };

    inline void parse_note(
        std::vector<NoteEvent>&out,
        std::istream&csv,
        std::vector<std::string>*const warnings = nullptr) noexcept {

        struct LineBreaker {
            LineBreaker(std::istream&i):i(i), str{}, eof(false) {}
            std::istream&i;
            std::string str;
            bool eof;
            void read_line() {
                if (!i.good()) {
                    eof = true;
                    return;
                }
                str.clear();
                for (char c; c = i.get(), i.good() && c != '\n';)
                    str.push_back(c);
                if (!str.empty() && str.back() == '\r') str.pop_back();
            }

            struct Iter {
                LineBreaker*l;
                bool operator==(std::nullptr_t) const noexcept { return l->eof; }
                Iter&operator++() { l->read_line(); return *this; }
                void operator++(int) { ++*this; }
                std::string_view operator*() { return l->str; }
                [[maybe_unused]] typedef std::ptrdiff_t difference_type;
            };
            Iter begin() { read_line(); return {this}; }
            std::nullptr_t end() const noexcept { return nullptr; }
        };

        for (auto n_line = 0; auto const line : LineBreaker{csv}) {
            if (line.empty()) continue;
            auto const end = line.end();

            int64_t time;
            constexpr auto parse_time = +[](
                char const*const begin,
                char const*const end,
                int64_t&time_out) noexcept->std::from_chars_result {
                auto time = 0LL;
                auto mili = true;

                auto c = begin;
                while (c < end) {
                    if (*c >= '0' && *c <= '9') {
                        for (auto cc = c + 1 ; cc < end; ++cc) {
                            if (*cc == '.') {
                                float tf;
                                auto const[cend, ec] = std::from_chars(c, end, tf);
                                if (ec == std::errc{}) {
                                    if (tf > 100.0f) tf = 100.0f;
                                    else if (tf < 0.0) tf = 0.0f;
                                    time_out = time * 1000 + static_cast<int64_t>(std::roundf(tf * 1000));
                                    return {cend, {}};
                                }
                                else if (c > begin) {
                                    time_out = mili ? time : time * 1000;
                                    return {c, {}};
                                }
                                else return {begin, ec};
                            }
                            else if (*cc < '0' || *cc > '9') break;
                        }
                        
                        int64_t ti;
                        auto const[cend, ec] = std::from_chars(c, end, ti);
                        if (ec == std::errc{}) {
                            time += ti;
                            c = cend;
                        }
                        else if (c > begin) {
                            time_out = mili ? time : time * 1000;
                            return {c, {}};
                        }
                        else return {begin, ec};
                    }
                    else if (*c == ':') {
                        mili = false;
                        time *= 60;
                        c += 1;
                    }
                    else break;
                }

                if (c > begin) {
                    time_out = mili ? time : time * 1000;
                    return {c, {}};
                }
                else return {begin, std::errc::invalid_argument};
            };
            auto const[cend_t, ec_t] = parse_time(line.begin(), end, time);
            if (ec_t != std::errc{}) {
                if (warnings)
                    warnings->push_back(std::format("Line {}: bad time, code {}", n_line, (int)ec_t));
                continue;
            }
            if (cend_t >= end) continue;
            if (*cend_t != ',') {
                if (warnings)
                    warnings->push_back(std::format("Line {}: bad note, missing comma", n_line));
                continue;
            }

            for (auto c = cend_t + 1; c < end;) {
                constexpr auto parse_single_note = +[](
                    char const*const begin,
                    char const*const end,
                    NoteEvent&note_out) noexcept->std::from_chars_result {
                    if (begin >= end) return {begin, std::errc::invalid_argument};

                    float x;
                    char const*c;
                    if (*begin == '(') {
                        for (auto cc = begin + 1;; ++cc) {
                            if (cc >= end) return {begin, std::errc::invalid_argument};
                            if (*cc == ')') {
                                auto const ec = std::from_chars(begin + 1, cc, x).ec;
                                if (ec != std::errc{}) return {begin, ec};
                                c = cc + 1;
                                break;
                            }
                        }
                    }
                    else if (*begin >= '0' && *begin <= '9') {
                        x = static_cast<float>(*begin - '0');
                        c = begin + 1;
                    }
                    else return {begin, std::errc::invalid_argument};

                    auto kind = NoteEvent::Kind::NORM;
                    auto dexterity = NoteEvent::Dexterity::EITHER;

                    for (;c < end; ++c) {
                        switch (*c) {
                        default: goto good;
                        case 's': case 'S':
                            dexterity = NoteEvent::Dexterity::LEFT;
                            break;
                        case 'd': case 'D':
                            dexterity = NoteEvent::Dexterity::RIGHT;
                            break;
                        case '+':
                            kind = NoteEvent::Kind::START;
                            break;
                        case '-':
                            kind = NoteEvent::Kind::END;
                            break;
                        case '=':
                            kind = NoteEvent::Kind::HOLD;
                            break;
                        case 'u': case 'U':
                            kind = NoteEvent::Kind::FORTH;
                            break;
                        case 'l': case 'L':
                            kind = NoteEvent::Kind::LEFT;
                            break;
                        case 'r': case 'R':
                            kind = NoteEvent::Kind::RIGHT;
                            break;
                        }
                    }

                good:
                    note_out.x = x;
                    note_out.kind = kind;
                    note_out.dexterity = dexterity;
                    return {c, {}};
                };

                NoteEvent note;
                note.time = time;
                auto const[cend_n, ec_n] = parse_single_note(c, end, note);
                if (ec_n != std::errc{}) {
                    if (warnings)
                        warnings->push_back(std::format("Line {}: bad note, near \"{}\"", n_line, *c));
                    c += 1;
                    continue;
                }
                out.push_back(note);
                c = cend_n;
            }
            
            n_line += 1;
        }
    }

    inline void note_to_mo(
        std::vector<MotionEvent>&out,
        std::span<NoteEvent const>const notes,
        ScrnMapper const&mapper,
        std::vector<std::string>*const warnings = nullptr) noexcept {
        auto const x_left = (static_cast<float>(mapper.num) + 1.0f) / 2.0f;
        constexpr auto DOWNMOVE = static_cast<MotionEvent::Kind>(8);
        out.clear();
        for (NoteEvent const note : notes) {
            auto const pos = mapper.map(note.x);
            auto const dex =
                note.dexterity == NoteEvent::Dexterity::LEFT ? 0 :
                note.dexterity == NoteEvent::Dexterity::RIGHT ? 1 :
                note.x < x_left ? 0 : 1;
            switch (note.kind) {
            default:
            case NoteEvent::Kind::NORM:
                out.emplace_back(note.time, pos, MotionEvent::Kind::DOWN, dex);
                out.emplace_back(note.time + 5, pos, MotionEvent::Kind::UP, dex);
                break;
            case NoteEvent::Kind::START:
                out.emplace_back(note.time, pos, MotionEvent::Kind::DOWN, dex);
                break;
            case NoteEvent::Kind::HOLD:
                out.emplace_back(note.time, pos, MotionEvent::Kind::MOVE, dex);
                break;
            case NoteEvent::Kind::END:
                out.emplace_back(note.time, pos, MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 5, pos, MotionEvent::Kind::UP, dex);
                break;
            case NoteEvent::Kind::FORTH:
                out.emplace_back(note.time - 20, pos, DOWNMOVE, dex);
                out.emplace_back(note.time, pos, MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 5, pos + mapper.offset(0, -mapper.touch_slop), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 10, pos + mapper.offset(0, -mapper.touch_slop * 2), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 15, pos + mapper.offset(0, -mapper.touch_slop * 3), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 20, pos + mapper.offset(0, -mapper.touch_slop * 4), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 22, pos + mapper.offset(0, -mapper.touch_slop * 4), MotionEvent::Kind::UP, dex);
                break;
            case NoteEvent::Kind::LEFT:
                out.emplace_back(note.time - 20, pos, DOWNMOVE, dex);
                out.emplace_back(note.time, pos, MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 5, pos + mapper.offset(-mapper.touch_slop, 0), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 10, pos + mapper.offset(-mapper.touch_slop * 2, 0), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 15, pos + mapper.offset(-mapper.touch_slop * 3, 0), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 20, pos + mapper.offset(-mapper.touch_slop * 4, 0), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 22, pos + mapper.offset(-mapper.touch_slop * 4, 0), MotionEvent::Kind::UP, dex);
                break;
            case NoteEvent::Kind::RIGHT:
                out.emplace_back(note.time - 20, pos, DOWNMOVE, dex);
                out.emplace_back(note.time, pos, MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 5, pos + mapper.offset(mapper.touch_slop, 0), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 10, pos + mapper.offset(mapper.touch_slop * 2, 0), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 15, pos + mapper.offset(mapper.touch_slop * 3, 0), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 20, pos + mapper.offset(mapper.touch_slop * 4, 0), MotionEvent::Kind::MOVE, dex);
                out.emplace_back(note.time + 22, pos + mapper.offset(mapper.touch_slop * 4, 0), MotionEvent::Kind::UP, dex);
                break;
            }
        }
        std::ranges::sort(out, {}, +[](ongearm::MotionEvent&m) { return m.time; });

        bool down[] = {false, false};
        for (size_t i = 0; i < out.size(); ++i) {
            auto const mo = out[i];
            auto dex = mo.dex;
            if (mo.kind == MotionEvent::Kind::UP) {
                if (down[dex]) down[dex] = false;
                else {
                    if (warnings)
                        warnings->push_back(std::format("Time {}: up when not down", mo.time));
                    out.emplace(out.begin() + i, mo.time, mo.pos, MotionEvent::Kind::DOWN, mo.dex);
                    i += 1;
                }
            }
            else if (mo.kind == MotionEvent::Kind::MOVE || mo.kind == DOWNMOVE) {
                if (down[dex]) {
                    out[i].kind = MotionEvent::Kind::MOVE;

                    // interpolate move motion events
                    auto i_prev = i;
                    while (i_prev > 0 && out[i_prev - 1].dex != dex)
                        i_prev -= 1;
                    if (i_prev <= 0) continue;
                    auto const mo_prev = out[i_prev - 1];
                    auto const t1 = static_cast<float>(mo_prev.time);
                    auto const t2 = static_cast<float>(mo.time);
                    auto const x1 = mapper.remap(mo_prev.pos);
                    auto const x2 = mapper.remap(mo.pos);
                    if (std::fabs(x2 - x1) < 0.125f) continue;

                    auto x_prev = x1;
                    if (x1 < x2) {
                        auto border = std::ceil((x1 - 0.25f) * 2.0f) / 2.0f + 0.25f;
                        for (;;) {
                            if (border > x2) border = x2;
                            auto x_end = border + 0.25;
                            if (x_end > x2) x_end = x2;
                            auto t = static_cast<int64_t>(std::lerp(t1, t2, (border - x1) / (x2 - x1)));
                            
                            for (auto x = std::ceil(x_prev * 8.0f) / 8.0f; x <= x_end; x += 0.125f, t += 5) {
                                if (out[i].time <= t) break;
                                out.emplace(out.begin() + i, t, mapper.map(x), MotionEvent::Kind::MOVE, mo.dex);
                                i += 1;
                            }
                            if (x_end >= x2) break;
                            x_prev = x_end;
                            border += 0.5f;
                        }
                    }
                    else {
                        auto border = std::floor((x1 - 0.25f) * 2.0f) / 2.0f + 0.25f;
                        for (;;) {
                            if (border < x2) border = x2;
                            auto x_end = border - 0.25;
                            if (x_end < x2) x_end = x2;
                            auto t = static_cast<int64_t>(std::lerp(t1, t2, (border - x1) / (x2 - x1)));
                            
                            for (auto x = std::floor(x_prev * 8.0f) / 8.0f; x >= x_end; x -= 0.125f, t += 5) {
                                if (out[i].time <= t) break;
                                out.emplace(out.begin() + i, t, mapper.map(x), MotionEvent::Kind::MOVE, mo.dex);
                                i += 1;
                            }
                            if (x_end <= x2) break;
                            x_prev = x_end;
                            border -= 0.5f;
                        }
                    }
                }
                else {
                    if (mo.kind == MotionEvent::Kind::MOVE) {
                        if (warnings)
                            warnings->push_back(std::format("Time {}: move when not down", mo.time));
                    }
                    out[i].kind = MotionEvent::Kind::DOWN;
                    down[dex] = true;
                }
            }
            else {
                if (down[dex]) {
                    if (warnings)
                        warnings->push_back(std::format("Time {}: down when down", mo.time));
                    out.emplace(out.begin() + i + 1, mo.time, mo.pos, MotionEvent::Kind::UP, mo.dex);
                    i += 1;
                }
                else down[dex] = true;
            }
        }

        if (!out.empty()) {
            auto const m = out.back();
            if (down[0]) {
                if (warnings)
                    warnings->push_back("Left hand left down");
                out.emplace_back(
                    m.time + 5,
                    mapper.map(1.0f),
                    MotionEvent::Kind::UP,
                    0);
            }
            if (down[1]) {
                if (warnings)
                    warnings->push_back("Right hand left down");
                out.emplace_back(
                    m.time + 5,
                    mapper.map(1.0f),
                    MotionEvent::Kind::UP,
                    1);
            }
        }
    }
}