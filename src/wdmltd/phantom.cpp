#include"phantom.hpp"

#include<parser.hpp>

#include<cstring>
#include<algorithm>
#include<chrono>
#include<fstream>

enum class MltdCourse {
    MM, M4, M2, S2,
};

static MltdCourse get_course(std::filesystem::path const&path) noexcept {
    auto const stem = path.stem().string();
    if (stem.ends_with("_4m")) return MltdCourse::M4;
    if (stem.ends_with("_2m")) return MltdCourse::M2;
    if (stem.ends_with("_2s") || stem.ends_with("_2p"))
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
    std::ranges::sort(song, {}, +[](PhantomInput const&pe) {
        return pe.time;
    });
}

int PhantomSong::change(std::filesystem::path const&path, MltdSize const&msize) noexcept {
    clear();

    std::ifstream csv(path);
    if (!csv.good()) {
        warnings.push_back(std::format("cannot read {}", path.c_str()));
        return -1;
    }
    ongearm::parse_note(buf_n, csv, &warnings);

    ongearm::ScrnMapper mapper;
    switch (get_course(path)) {
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
    ongearm::note_to_mo(buf_m, buf_n, mapper, &warnings);

    make_phanton_macro(data, buf_m);
    return 0;
}

int play_phantom_macro(PlayMacroContext*const c) noexcept {
    struct Func {
        static int64_t now() noexcept {
            using namespace std::chrono;
            static_assert(high_resolution_clock::period::den >= 1000'000);
            return floor<milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
        }
    };
    auto&abrt = c->abrt;
    std::span song{*c->song};
    auto&ptsvr = *c->ptsvr;
    struct DoneGuard {
        std::atomic_bool&done;
        ~DoneGuard() {
            done.store(true, std::memory_order::relaxed);
        }
    } const _g0{c->done};
    
    if (song.empty()) return 0;
    auto const&[t1, s1, d1] = song.front();
    auto const tp_start = Func::now() - t1;
    ptsvr.write(d1, s1);
    ptsvr.flush();
    if (!ptsvr.good()) return -1;
    for (auto const&[t, s, d] : song.subspan(1)) {
        auto const dt = t - (Func::now() - tp_start);
        std::this_thread::sleep_for(std::chrono::milliseconds(dt));
        ptsvr.write(d, s);
        ptsvr.flush();
        if (!ptsvr.good()) return -1;
        if (abrt.load(std::memory_order::relaxed)) {
            ptsvr.put(PhantomInput::CMD_TOUCH_CANCEL);
            ptsvr.flush();
            return 0;
        }
    }
    return 0;
}