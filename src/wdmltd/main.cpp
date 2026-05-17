#include"phantom.hpp"

#include<boost/asio.hpp>
#include<toml.hpp>

extern "C" {
#include<linux/input.h>
}

#include<cstdlib>
#include<cstdio>
#include<algorithm>
#include<atomic>
#include<charconv>
#include<chrono>
#include<filesystem>
#include<format>
#include<fstream>
#include<optional>
#include<print>
#include<span>
#include<thread>
#include<vector>

struct TapCtrl {
    int key;
    int32_t x, y;
    bool repeat;
};

struct PlayMacroContext {
    std::atomic_bool abrt, done;
    boost::asio::ip::tcp::iostream*ptsvr;
    std::vector<PhantomInput>const*song;
};

static int play_phantom_macro(PlayMacroContext*const c) noexcept {
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

static void load_song(
    std::vector<PhantomInput>&song,
    std::string_view const song_name,
    MltdSize const&msize) {
    std::vector<std::string> warnings;
    change_song(song, song_name, msize, &warnings);
    for (auto&w : warnings) {
        std::println(stderr, "warning: {}", w);
    }
    std::println("loaded {} with +{} events", song_name, song.size());
}

static std::optional<MltdSize> msize_from_arg(std::string_view arg) noexcept {
    int32_t v_must[6];
    for (auto&v : v_must) {
        auto const[ptr, ec] = std::from_chars(arg.begin(), arg.end(), v);
        if (ec != std::errc{}) return {};
        if (ptr == arg.end()) {
            arg = {};
            continue;
        }
        if (*ptr != ',') return {};
        arg = std::string_view(ptr + 1, arg.end());
    }
    int32_t v_opt[3] = { v_must[0], v_must[1], 40 };
    for (auto&v : v_opt) {
        auto const[ptr, ec] = std::from_chars(arg.begin(), arg.end(), v);
        if (ec != std::errc{}) {
            if (arg.empty()) break;
            if (arg.front() != ',') return {};
            arg = arg.substr(1);
            continue;
        }
        if (ptr == arg.end()) break;
        if (*ptr != ',') return {};
        arg = std::string_view(ptr + 1, arg.end());
    }
    return MltdSize{
        v_must[0], v_must[1], v_must[2], v_must[3], v_must[4], v_must[5],
        v_opt[0], v_opt[1], v_opt[2],
    };
}

int main(int const argc, char**const argv) {
    std::string_view song_name;
    std::string_view config_name;
    std::string waydroid_ip;
    std::string kbd_path;
    std::optional<MltdSize> msize{};

    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--help") {
            std::println("not written yet.");
            return 0;
        }
        else if (arg == "--config") {
            i += 1;
            if (i >= argc)
                return std::print(stderr, "missing --config argument"), -2;
            if (std::filesystem::exists(argv[i])) config_name = argv[i];
            else
                return std::print(stderr, "config file {} does not exist", argv[i]), -2;
        }
        else if (arg == "--ip") {
            i += 1;
            if (i >= argc)
                return std::print(stderr, "missing --ip argument"), -2;
            waydroid_ip = argv[i];
        }
        else if (arg == "--kbd") {
            i += 1;
            if (i >= argc)
                return std::print(stderr, "missing --kbd argument"), -2;
            kbd_path = argv[i];
        }
        else if (arg == "--screen") {
            i += 1;
            if (i >= argc)
                return std::print(stderr, "missing --screen argument"), -2;
            msize = msize_from_arg(argv[i]);
            if (!msize.has_value())
                return std::print(stderr, "bad --screen argument \"{}\"", argv[i]), -2;
        }
        else {
            if (!song_name.empty())
                std::print(stderr, "song {} ignored. One song only", song_name);
            song_name = argv[i];
        }
    }

    auto const config =
        !config_name.empty() ? toml::parse_file(config_name) :
        std::filesystem::exists("config.toml") ? toml::parse_file("config.toml") :
        toml::parse_result{};

    if (waydroid_ip.empty()) {
        if (auto ip_or = config["ip"].value<std::string_view>(); ip_or.has_value() && ip_or != "auto")
            waydroid_ip = *ip_or;
        else {
            std::system("waydroid status|grep 'IP address'|cut -f 2 > /tmp/ongearm_waydroid_config");
            std::ifstream{"/tmp/ongearm_waydroid_config"} >> waydroid_ip;
            std::filesystem::remove("/tmp/ongearm_waydroid_config");
            if (waydroid_ip.empty())
                return std::println(stderr, "unable to retrieve Waydroid ip"), -1;
        }
    }
    if (kbd_path.empty()) {
        std::system("ls /dev/input/by-id/*-event-kbd -1|head -n 1 > /tmp/ongearm_waydroid_config");
        std::ifstream{"/tmp/ongearm_waydroid_config"} >> kbd_path;
        std::filesystem::remove("/tmp/ongearm_waydroid_config");
        if (kbd_path.empty())
            return std::println(stderr, "keyboard not found"), -1;
    }
    auto const waydroid_port =
        config["port"].value_or<std::string_view>("27183");
    auto const key_exit =
        config["key-exit"].value_or(KEY_PAUSE);
    auto const key_play =
        config["key-play"].value_or(KEY_Z);
    auto const song_path =
        config["song-path"].value_or<std::string_view>(".");

    if (!msize.has_value()) {
        if (auto const screen = config["screen"]; screen.is_table()) {
            msize = [screen]()->std::optional<MltdSize> {
                MltdSize msize;
                if (auto const v = screen["unit"]["x1"].value<int32_t>()) {
                    msize.x_left_unit = *v;
                    msize.x_left_2mix = screen["2mix"]["x1"].value_or(*v);
                }
                else return {};
                if (auto const v = screen["unit"]["x2"].value<int32_t>()) {
                    msize.x_right_unit = *v;
                    msize.x_right_2mix = screen["2mix"]["x2"].value_or(*v);
                }
                else return {};
                if (auto const v = screen["unit"]["y"].value<int32_t>())
                    msize.y_unit = *v;
                else return {};
                if (auto const v = screen["solo"]["x1"].value<int32_t>())
                    msize.x_left_solo = *v;
                else return {};
                if (auto const v = screen["solo"]["x2"].value<int32_t>())
                    msize.x_right_solo = *v;
                else return {};
                if (auto const v = screen["solo"]["y"].value<int32_t>())
                    msize.y_solo = *v;
                else return {};
                msize.slop = screen["slop"].value_or<int32_t>(40);
                return msize;
            }();
        }
    }

    struct Ctrl {
        int key;
        int32_t x, y;
        static int get_key(Ctrl const&c) { return c.key; }
    };
    std::vector<Ctrl> ctrls;
    if (auto const ctrls_config = config["ctrl"]; ctrls_config.is_array_of_tables()) {
        for (auto&ctrl_node : *ctrls_config.as_array()) {
            auto&ctrl = *ctrl_node.as_table();
            auto const key = ctrl["key"].value<int>();
            auto const x = ctrl["x"].value<int32_t>();
            auto const y = ctrl["y"].value<int32_t>();
            if (key && x && y) {
                ctrls.emplace_back(*key, *x, *y);
            }
        }
    }
    std::ranges::sort(ctrls, {}, Ctrl::get_key);

    boost::asio::ip::tcp::iostream ptsvr(waydroid_ip, waydroid_port);
    struct PtsvrGuard {
        std::string pid;
        std::thread th;
        ~PtsvrGuard() {
            if (!th.joinable()) return;
            std::println("stop PhantomServer");
            std::system(std::format("waydroid shell kill -- {}", pid).c_str());
            th.join();
        }
    } _g0{};
    if (ptsvr) std::println("connected to already running PhantomServer");
    else if (auto const ptsvr_path = config["ptsvr-path"].value<std::string_view>()) {
        std::thread th([ptsvr_path = *ptsvr_path, waydroid_port]() noexcept {
            std::system(std::format(
                "echo CLASSPATH={} app_process /data/local/tmp"
                " com.phantom.server.PhantomServer -- --port {}"
                "|waydroid shell sh", ptsvr_path, waydroid_port).c_str());
        });

        for (auto retry = 5;;) {
            std::system(
                "waydroid shell ps -- -A -o pid,args"
                "|grep PhantomServer"
                " > /tmp/ongearm_waydroid_config");
            std::string pid;
            std::ifstream("/tmp/ongearm_waydroid_config") >> pid;
            std::filesystem::remove("/tmp/ongearm_waydroid_config");
            auto const i0 = pid.find_first_of("0123456789");
            auto const i1 = pid.find_first_not_of("\t 0123456789");
            if (i0 != std::string::npos && i0 < i1) {
                _g0.pid = pid.substr(i0, i1 - i0);
                _g0.th = static_cast<std::thread&&>(th);
                std::println("PhantomServer started, pid {}", _g0.pid);
                break;
            }
            retry -= 1;
            if (retry > 0)
                std::this_thread::sleep_for(std::chrono::seconds{1});
            else {
                std::println(stderr, "unable to confirm PhantomServer process");
                th.detach();
                return -1;
            }
        }
        for (auto retry = 5;;) {
            ptsvr.clear();
            ptsvr.connect(waydroid_ip, waydroid_port);
            if (ptsvr) break;
            retry -= 1;
            if (retry > 0)
                std::this_thread::sleep_for(std::chrono::seconds{1});
            else return std::println(
                stderr,
                "unable to connect to the PhantonServer: {}",
                ptsvr.error().message()), -1;
        }
    }
    else return std::println(
        stderr,
        "unable to connect to the PhantonServer: {}\n"
        "and no known way to start it",
        ptsvr.error().message()), -1;
    std::ifstream kbd{kbd_path};
    std::println("watching {}", kbd_path);

    std::vector<PhantomInput> song;

    if (song_name.empty())
        std::println(stderr, "warning: no song loaded.");
    else if (!msize.has_value())
        std::println(stderr, "warning: no screen data, no song loaded.");
    else load_song(song, std::format("{}/{}", song_path, song_name), *msize);

    std::system("stty -echo");
    struct EchoGuard {
        ~EchoGuard() { std::system("stty echo"); }
    } const _g1;

    input_event ev;
    std::thread th_play;
    PlayMacroContext playctx{false, false, &ptsvr, &song};
    for (;;) {
        if (kbd.read(reinterpret_cast<char*>(&ev), sizeof(ev)), !kbd.good())
            return std::println(stderr, "failed to read keyboard input"), -1;
        if (ev.type != EV_KEY) continue;

        if (th_play.joinable()) {
            if (playctx.done.load(std::memory_order::relaxed)) {
                th_play.join();
                if (!ptsvr)
                    return std::print(stderr, "Phantom server closed."), -1;
                playctx.done.store(false, std::memory_order::relaxed);
            }
            else {
                if (ev.code == key_exit && ev.value == 1) {
                    playctx.abrt.store(true, std::memory_order::relaxed);
                    th_play.join();
                    if (!ptsvr)
                        return std::print(stderr, "Phantom server closed."), -1;
                    playctx.abrt.store(false, std::memory_order::relaxed);
                    playctx.done.store(false, std::memory_order::relaxed);
                }
                continue;
            }
        }

        if (ev.value == 1) {
            if (ev.code == key_exit) break;
            else if (ev.code == key_play) {
                th_play = std::thread(play_phantom_macro, &playctx);
                std::println("starting playing the song");
                std::println("press exit key to stop");
            }
            else if (auto const i = std::ranges::lower_bound(
                ctrls, ev.code, {}, Ctrl::get_key); i != ctrls.end() && i->key == ev.code) {
                ptsvr.put(PhantomInput::CMD_TOUCH_DOWN);
                ptsvr.put(0);
                ptsvr.write(reinterpret_cast<char const*>(&i->x), 4);
                ptsvr.write(reinterpret_cast<char const*>(&i->y), 4);
                ptsvr.put(PhantomInput::CMD_TOUCH_UP);
                ptsvr.put(0);
                ptsvr.flush();
                if (!ptsvr)
                    return std::print(stderr, "Phantom server closed."), -1;
            }
        }
    }
}