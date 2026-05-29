#include"config.hpp"
#include"ctrl.hpp"
#include"phantom.hpp"
#include"ptsvr.hpp"
#include"recept.hpp"

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
#include<system_error>
#include<thread>
#include<utility>
#include<vector>

static int load_song(
    std::vector<PhantomInput>&song,
    Config const&conf,
    std::string_view const name) {
    std::error_code ec;
    if (!conf.msize_ok)
        std::println(stderr, "warning: no screen data");
    else if (auto const song_fullname = std::filesystem::path{
        conf.song_dir
    }.append(name); std::filesystem::exists(song_fullname, ec)) {
        std::vector<std::string> warnings;
        change_song(song, song_fullname, conf.msize, &warnings);
        for (auto&w : warnings) {
            std::println(stderr, "warning: {}", w);
        }
        std::println("loaded {} with {:+} events", name, song.size());
    }
    else if (ec)
        return std::println(stderr, "fs: {}", ec.message()), ec.value();
    else {
        std::println(stderr, "warning: {} does not exist", conf.song);
    }
    return 0;
}

namespace cli {
    int daemon(int const argc, char**const argv) noexcept {
        Config conf(argc, argv);
        if (!conf.good) return -2;
        std::error_code ec;

        boost::asio::ip::tcp::iostream ptsvr;
        PtsvrConnection const cnct(
            ptsvr, conf.phantom_ip, conf.phantom_port, conf.ptsvr_jar);
        if (!ptsvr.good()) {
            auto&err = ptsvr.error();
            std::println(
                stderr,
                "unable to connect to the PhantonServer: {}",
                err.message());
            return err.value();
        }

        std::filesystem::path kbd;
        if (conf.kbd.empty()) {
            std::filesystem::directory_iterator
                dev("/dev/input/by-id", ec), end;
            if (ec) return std::println(
                stderr, "input dev error: {}", ec.message()), ec.value();
            for (;;) {
                if (dev == end)
                    return std::println(stderr, "keyboard not found"), -1;
                auto&p = dev->path();
                auto const p_str = p.string();
                if (p_str.ends_with("-event-kbd")) {
                    kbd = p;
                    std::println("watching {}", p_str);
                    break;
                }
                dev.increment(ec);
                if (ec) return std::println(
                    stderr, "input dev error: {}", ec.message()), ec.value();
            }
        }
        else {
            kbd = conf.kbd;
            std::println("watching {}", conf.kbd);
        }

        std::vector<PhantomInput> song;
        if (!conf.song.empty()) {
            if (auto const err = load_song(song, conf, conf.song); err != 0)
                return err;
        }

        Ctrls ctrls;
        std::println("{} controls loaded", conf.ctrls.size());
        ctrls.assign(std::move(conf.ctrls));

        std::system("stty -echo");
        struct EchoGuard {
            ~EchoGuard() { std::system("stty echo"); }
        } const _g1;

        Recept rc(conf.daemon_port, kbd);
        std::thread th_play;
        PlayMacroContext playctx{false, false, &ptsvr, &song};
        auto paused = false;
        for (;;) {
            auto const e = rc.poll();

            if (std::holds_alternative<Recept::Err>(e)) {
                if (th_play.joinable()) {
                    playctx.abrt.store(true, std::memory_order::relaxed);
                    th_play.join();
                }

                auto&err = std::get<Recept::Err>(e);
                std::println(stderr, "error: {}", err.message());
                return err.value();
            }

            else if (std::holds_alternative<Recept::Command>(e)) {
                if (th_play.joinable()) continue;
                auto&cmd = std::get<Recept::Command>(e);

                if (cmd.kind == Recept::Command::QUIT)
                    return 0;
                else if (cmd.kind == Recept::Command::PAUSE) {
                    paused = true;
                    std::println("paused");
                }
                else if (cmd.kind == Recept::Command::RESUME) {
                    paused = false;
                    std::println("resumed");
                }
                else if (cmd.kind == Recept::Command::SONG) {
                    std::string p(cmd.data.size(), '\0');
                    std::memcpy(p.data(), cmd.data.data(), cmd.data.size());
                    if (auto const err = load_song(song, conf, p); err != 0) {
                        std::println("load song failed: {}", err);
                        song.clear();
                    }
                }
                else if (cmd.kind == Recept::Command::CONFIG) {
                    std::string p(cmd.data.size(), '\0');
                    std::memcpy(p.data(), cmd.data.data(), cmd.data.size());
                    if (auto const err = conf.load(p); err != 0) {
                        std::println("load config failed: {}", err);
                    }
                    else {
                        std::println("{} controls loaded", conf.ctrls.size());
                        ctrls.assign(std::move(conf.ctrls));
                    }
                }
            }

            else if (std::holds_alternative<Recept::Input>(e)) {
                auto&ev = std::get<Recept::Input>(e);

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
                    if (ev.code == conf.key_exit) {
                        paused ^= true;
                        std::println("{}", paused ? "paused" : "resumed");
                        continue;
                    }
                    else if (paused) continue;
                    else if (ev.code == conf.key_play) {
                        th_play = std::thread(play_phantom_macro, &playctx);
                        std::println("starting playing the song");
                        std::println("press exit key to stop");
                        continue;
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
    }
}
