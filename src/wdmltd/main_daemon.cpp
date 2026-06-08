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
#include<iostream>
#include<print>
#include<span>
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
    if (!conf.msize_ok) {
        std::println(stderr, "error: no screen data");
        return -2;
    }
    if (std::filesystem::exists(name, ec)) {
        std::vector<std::string> warnings;
        change_song(song, name, conf.msize, &warnings);
        for (auto&w : warnings) {
            std::println(stderr, "warning: {}", w);
        }
        std::println("loaded {} with {:+} events", name, song.size());
    }
    else if (ec) {
        std::println(stderr, "fserr: {}", ec.message());
        return ec.value();
    }
    else {
        std::println(stderr, "not exist: {}", name);
        return -2;
    }
    return 0;
}

static void load_ctrls(CtrlPanel&ctrls, std::span<CtrlPanel::Ctrl const>const cs) noexcept {
    auto const n = ctrls.assign(cs);
    std::println("{} controls loaded", n);
    if (n < cs.size())
        std::println(stderr, "warning: {} controls not loaded", cs.size() - n);
}

static int select_kbd(std::filesystem::path&kbd) {
    std::error_code ec;
    std::vector<std::filesystem::path> kbds;
    for (std::filesystem::directory_iterator dev("/dev/input/by-id", ec), end;;) {
        if (ec) return std::println(
            stderr, "error: {}", ec.message()), ec.value();
        if (dev == end) break;
        auto&p = dev->path();
        if (std::string_view{p.c_str()}.ends_with("-event-kbd"))
            kbds.push_back(p);
        dev.increment(ec);
    }
    if (kbds.empty())
        return std::println(stderr, "keyboard not found"), -1;
    else if (kbds.size() == 1) kbd = kbds.front();
    else {
        for (size_t n = 0; auto&p : kbds) {
            std::println("{}. {}", n, p.c_str());
            n += 1;
        }
        std::print("select:");
        size_t n;
        std::cin >> n;
        if (n >= kbds.size()) {
            std::println(stderr, "invalid selection");
            return -2;
        }
        kbd = kbds[n];
    }
    return 0;
}

namespace cli {
    int daemon(int const argc, char**const argv) noexcept {
        Args const args(argc, argv, 1);
        Config conf(args);
        if (!conf.err.empty()) {
            std::println(stderr, "{}", conf.err);
            return -2;
        }
        std::error_code ec;

        std::filesystem::path kbd;
        if (args.kbd.empty()) {
            if (auto const err = select_kbd(kbd); err != 0)
                return err;
        }
        else kbd = args.kbd;
        std::println("watching {}", kbd.c_str());

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

        std::vector<PhantomInput> song;

        boost::asio::io_context ctx;
        CtrlPanel ctrls{ctx};
        load_ctrls(ctrls, conf.ctrls);

        Recept rc(ctx, conf.daemon_port, kbd);
        std::thread th_play;
        PlayMacroContext playctx{false, false, &ptsvr, &song};
        auto paused = false;

        for (Job e;;) {
            e.kind = Job::NONE;
            
            for (;;) {
                rc.poll(e);
                if (e.kind != Job::NONE) break;
                ctrls.poll(e);
                if (e.kind != Job::NONE) break;
                
                ctx.run_one();
                ctx.restart();
            }
            
            if (th_play.joinable() && playctx.done.load(std::memory_order::relaxed)) {
                th_play.join();
                if (!ptsvr)
                    return std::println(stderr, "Phantom server closed unexpectedly: {}",
                        ptsvr.error().message()), -1;
                playctx.done.store(false, std::memory_order::relaxed);
            }

            if (e.kind == Job::ERR) {
                if (th_play.joinable()) {
                    playctx.abrt.store(true, std::memory_order::relaxed);
                    th_play.join();
                }

                std::println(stderr, "error: {}", e.err);
                return -1;
            }

            else if (e.kind == Job::CMD) {
                auto&cmd = e.cmd;

                if (cmd.kind == Job::Cmd::QUIT) {
                    if (th_play.joinable()) {
                        playctx.abrt.store(true, std::memory_order::relaxed);
                        th_play.join();
                    }
                    return 0;
                }
                else if (cmd.kind == Job::Cmd::PAUSE) {
                    paused = true;
                    std::println("paused");
                }
                else if (cmd.kind == Job::Cmd::RESUME) {
                    paused = false;
                    std::println("resumed");
                }
                else if (cmd.kind == Job::Cmd::SONG) {
                    if (th_play.joinable()) {
                        std::println(stderr, "song playing, new song not loaded.");
                    }
                    else {
                        std::string p(cmd.data.size(), '\0');
                        std::memcpy(p.data(), cmd.data.data(), cmd.data.size());
                        if (auto const err = load_song(song, conf, p); err != 0) {
                            std::println(stderr, "load song failed: {}", err);
                            song.clear();
                        }
                    }
                }
                else if (cmd.kind == Job::Cmd::CONFIG) {
                    std::string p(cmd.data.size(), '\0');
                    std::memcpy(p.data(), cmd.data.data(), cmd.data.size());
                    if (auto const err = conf.load(p); !err.empty()) {
                        std::println("load config failed: {}", err);
                    }
                    else {
                        song.clear();
                        load_ctrls(ctrls, conf.ctrls);
                    }
                }
            }

            else if (e.kind == Job::KEY) {
                auto&ev = e.ev;
                ctrls.input(ev.code, ev.value);
            }

            else if (e.kind == Job::PAUSE) {
                if (th_play.joinable()) {
                    playctx.abrt.store(true, std::memory_order::relaxed);
                    th_play.join();
                    if (!ptsvr)
                        return std::println(stderr, "Phantom server closed unexpectedly: {}",
                            ptsvr.error().message()), -1;
                    playctx.abrt.store(false, std::memory_order::relaxed);
                    playctx.done.store(false, std::memory_order::relaxed);
                    std::println("song stopped");
                }
                else {
                    paused ^= true;
                    std::println("{}", paused ? "paused" : "resumed");
                }
            }
            
            else if (e.kind == Job::SONG) {
                if (!paused && !th_play.joinable()) {
                    th_play = std::thread(play_phantom_macro, &playctx);
                    std::println("starting playing the song");
                    std::println("press exit key to stop");
                }
            }

            else if (e.kind == Job::MOTION) {
                if (!paused && !th_play.joinable()) {
                    auto&o = e.mdata;
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
