#include"phantom.hpp"

#include<parser.hpp>

#include<cstring>
#include<algorithm>
#include<fstream>

enum class MltdCourse {
    MM, M4, M2, S2,
};

static MltdCourse get_course(std::string_view name) noexcept {
    if (name.ends_with(".csv"))
        name = name.substr(0, name.size() - 4);
    if (name.ends_with("_4m")) return MltdCourse::M4;
    if (name.ends_with("_2m")) return MltdCourse::M2;
    if (name.ends_with("_2s") || name.ends_with("_2p"))
        return MltdCourse::S2;
    return MltdCourse::MM;
}

static void make_phanton_macro(
    std::vector<PhantomInput>&song,
    std::span<ongearm::MotionEvent const>const motions) noexcept {
    for (auto m : motions) {
        auto&[t, s, d] = song.emplace_back(PhantomInput{});
        t = m.time;
        switch (m.kind) {
        case ongearm::MotionEvent::Kind::DOWN:
            s = 10;
            d[0] = PhantomInput::CMD_TOUCH_DOWN;
            d[1] = m.dex + 1;
            std::memcpy(d + 2, &m.pos.x, 4);
            std::memcpy(d + 6, &m.pos.y, 4);
            break;
        case ongearm::MotionEvent::Kind::MOVE:
            s = 10;
            d[0] = PhantomInput::CMD_TOUCH_MOVE;
            d[1] = m.dex + 1;
            std::memcpy(d + 2, &m.pos.x, 4);
            std::memcpy(d + 6, &m.pos.y, 4);
            break;
        case ongearm::MotionEvent::Kind::UP:
            s = 2;
            d[0] = PhantomInput::CMD_TOUCH_UP;
            d[1] = m.dex + 1;
            break;
        default:
            s = 1;
            d[0] = PhantomInput::CMD_TOUCH_CANCEL;
        }
    }
    std::ranges::sort(song, +[](PhantomInput const&pe1, PhantomInput const&pe2) {
        return pe1.time < pe2.time;
    });
}

int change_song(
    std::vector<PhantomInput>&song,
    std::string_view const name,
    MltdSize const&msize,
    std::vector<std::string>*const warnings) noexcept {
    song.clear();
    std::ifstream csv(std::string{name});
    if (!csv.good()) {
        if (warnings)
            warnings->push_back(std::format("cannot read {}", name));
        return -1;
    }
    auto const notes = ongearm::parse_note(csv, warnings);

    ongearm::ScrnMapper mapper;
    switch (get_course(name)) {
    default:
    case MltdCourse::MM:
        mapper.left = msize.x_left_unit;
        mapper.right = msize.x_right_unit;
        mapper.num = 6;
    hor_common:
        mapper.y = msize.y_unit;
        mapper.orient = ongearm::ScrnMapper::Orient::R0;
        break;
    case MltdCourse::M4:
        mapper.left = msize.x_left_unit;
        mapper.right = msize.x_right_unit;
        mapper.num = 4;
        goto hor_common;
    case MltdCourse::M2:
        mapper.left = msize.x_left_2mix;
        mapper.right = msize.x_right_2mix;
        mapper.num = 2;
        goto hor_common;
    case MltdCourse::S2:
        mapper.left = msize.x_left_solo;
        mapper.right = msize.x_right_solo;
        mapper.y = msize.y_solo;
        mapper.orient = ongearm::ScrnMapper::Orient::R90;
        mapper.num = 2;
        break;
    }
    mapper.touch_slop = msize.slop;
    auto const motions = ongearm::note_to_mo(notes, mapper, warnings);

    make_phanton_macro(song, motions);
    return 0;
}