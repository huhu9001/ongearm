#include"config.hpp"
#include"ctrl.hpp"
#include"phantom.hpp"
#include"ptsvr.hpp"

#include<boost/asio.hpp>

extern "C" {
#include<linux/input.h>
}

#include<cstdlib>
#include<cstdio>
#include<charconv>
#include<filesystem>
#include<fstream>
#include<print>
#include<string_view>
#include<thread>
#include<vector>

int main(int const argc, char**const argv) {
    if (argc >= 2 && std::string_view{argv[1]} == "help") {
        std::println("not written yet");
        return 0;
    }

    Config conf(argc, argv);
    if (!conf.good) return -2;

    boost::asio::ip::tcp::iostream ptsvr;
    auto const _g0 = Ptsvr::connect(
        ptsvr, conf.ip, conf.port, conf.ptsvr_jar);
    if (!ptsvr) return std::println(
        stderr,
        "unable to connect to the PhantonServer: {}",
        ptsvr.error().message()), -1;

    std::ifstream kbd{conf.kbd};
    if (conf.kbd.empty()) {
        std::string kbd_detected;
        std::system("ls /dev/input/by-id/*-event-kbd -1|head -n 1 > /tmp/ongearm_waydroid_config");
        std::ifstream{"/tmp/ongearm_waydroid_config"} >> kbd_detected;
        std::filesystem::remove("/tmp/ongearm_waydroid_config");
        if (kbd_detected.empty())
            return std::println(stderr, "keyboard not found"), -1;
        kbd.open(kbd_detected);
        std::println("watching {}", kbd_detected);
    }
    else {
        kbd.open(conf.kbd);
        std::println("watching {}", conf.kbd);
    }

    std::vector<PhantomInput> song;
    if (!conf.song.empty()) {
        if (!conf.msize_ok)
            std::println(stderr, "warning: no screen data");
        else if (auto const song_fullname = std::filesystem::path{
            conf.song_dir
        }.append(conf.song); std::filesystem::exists(song_fullname)) {
            std::vector<std::string> warnings;
            change_song(song, song_fullname, conf.msize, &warnings);
            for (auto&w : warnings) {
                std::println(stderr, "warning: {}", w);
            }
            std::println("loaded {} with {:+} events", conf.song, song.size());
        }
        else std::println(stderr, "warning: {} does not exist", conf.song);
    }

    Ctrls ctrls;
    std::println("{} controls loaded", conf.ctrls.size());
    ctrls.assign(static_cast<decltype(conf.ctrls)&&>(conf.ctrls));

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
                    return std::println(stderr, "Phantom server closed unexpectedly: {}",
                        ptsvr.error().message()), -1;
                playctx.done.store(false, std::memory_order::relaxed);
            }
            else {
                if (ev.code == conf.key_exit && ev.value == 1) {
                    playctx.abrt.store(true, std::memory_order::relaxed);
                    th_play.join();
                    if (!ptsvr)
                        return std::println(stderr, "Phantom server closed unexpectedly: {}",
                            ptsvr.error().message()), -1;
                    playctx.abrt.store(false, std::memory_order::relaxed);
                    playctx.done.store(false, std::memory_order::relaxed);
                    std::println("song stopped");
                }
                continue;
            }
        }

        if (ev.value == 1) {
            if (ev.code == conf.key_exit) break;
            else if (ev.code == conf.key_play) {
                th_play = std::thread(play_phantom_macro, &playctx);
                std::println("starting playing the song");
                std::println("press exit key to stop");
            }
        }

        if (auto const o = ctrls.input(ev.code, ev.value); !o.empty()) {
            ptsvr.write(reinterpret_cast<char const*>(o.data()), o.size());
            ptsvr.flush();
            if (!ptsvr)
                return std::println(stderr, "Phantom server closed unexpectedly: {}",
                    ptsvr.error().message()), -1;
        }
    }
}