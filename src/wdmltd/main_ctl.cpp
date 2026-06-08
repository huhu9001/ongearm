#include"config.hpp"
#include"recept.hpp"

#include<boost/asio.hpp>

#include<format>
#include<iostream>
#include<print>
#include<string_view>
#include<utility>
#include<vector>

namespace cli {
    int cmds(int const argc, char**const argv, uint8_t const cmd) noexcept {
        Args args(argc, argv, 1);
        if (!args.err.empty()) {
            std::println(stderr, "{}", args.err);
            return -2;
        }
        std::string port{args.port};
        if (port.empty()) {
            Config const conf(args);
            if (!conf.err.empty()) {
                std::println(stderr, "{}", conf.err);
                return -2;
            }
            port = std::format("{}", conf.daemon_port);
        }
        
        boost::asio::ip::tcp::iostream s("127.0.0.1", port);
        if (!s.good()) {
            std::println(stderr, "can not connect");
            return -1;
        }
        s.put(cmd).put(0).flush();
        if (!s.good()) {
            std::println(stderr, "can not send data");
            return -1;
        }
        return 0;
    }

    int song(int const argc, char**const argv) noexcept {
        Args const args(argc, argv, 1);
        Config const conf(args);
        if (!conf.err.empty()) {
            std::println(stderr, "{}", conf.err);
            return -2;
        }
        auto const port = std::format("{}", conf.daemon_port);

        if (args.inputs.empty()) {
            std::println("remove song");
            boost::asio::ip::tcp::iostream s("127.0.0.1", port);
            if (!s.good()) {
                std::println(stderr, "can not connect");
                return -1;
            }
            s.put(Job::Cmd::SONG).put(0).flush();
            if (!s.good()) {
                std::println(stderr, "can not send data");
                return -1;
            }
        }
        else {
            std::filesystem::path song_name{args.inputs.front()};
            auto const song_filename = song_name.filename();
            auto const song_parent = song_name.has_parent_path() ?
                song_name.parent_path()
            : std::filesystem::path{conf.song_dir};
            if (std::string_view nm{song_filename.c_str()}; nm.contains('*')) {
                auto const head = nm.substr(0, nm.find('*'));
                auto const tail = nm.substr(nm.rfind('*') + 1);
                std::vector<std::filesystem::path> candi;
                std::error_code ec;
                for (std::filesystem::directory_iterator dir(song_parent, ec), end;;) {
                    if (ec) {
                        std::println(stderr, "fserr: {}", ec.message());
                        return ec.value();
                    }
                    if (dir == end) break;
                    auto&p = dir->path();
                    auto const stem = p.stem();
                    std::string_view const s{stem.c_str()};
                    if (s.starts_with(head) && s.ends_with(tail))
                        candi.push_back(p);
                    dir.increment(ec);
                }

                if (candi.empty()) {
                    std::println(stderr, "not exist: {}", nm);
                    return -2;
                }
                else if (candi.size() == 1) {
                    const auto abs = std::filesystem::absolute(candi.front(), ec);
                    if (ec) {
                        std::println(stderr, "fserr: {}", ec.message());
                        return ec.value();
                    }
                    song_name = abs;
                }
                else {
                    for (size_t n = 0; auto&p : candi) {
                        std::println("{}. {}", n, p.c_str());
                        n += 1;
                    }
                    std::print("select:");
                    size_t n;
                    std::cin >> n;
                    if (n >= candi.size()) {
                        std::println(stderr, "invalid selection");
                        return -2;
                    }
                    const auto abs = std::filesystem::absolute(candi[n], ec);
                    if (ec) {
                        std::println(stderr, "fserr: {}", ec.message());
                        return ec.value();
                    }
                    song_name = abs;
                }
            }
            else {
                song_name = song_parent / song_filename;
                std::error_code ec;
                if (!std::filesystem::exists(song_name, ec)) {
                    if (ec) {
                        std::println(stderr, "fserr: {}", ec.message());
                        return ec.value();
                    }
                    else {
                        std::println(stderr, "not exist: {}", song_name.c_str());
                        return -2;
                    }
                }
                const auto abs = std::filesystem::absolute(song_name, ec);
                if (ec) {
                    std::println(stderr, "fserr: {}", ec.message());
                    return ec.value();
                }
                song_name = abs;
            }

            std::string_view const sn{song_name.c_str()};
            std::println("change song: {}", sn);
            auto const size = sn.size();
            if (size >= 0xFF) {
                std::println(stderr, "song path too long (>255 char): {}", sn);
                return -1;
            }

            boost::asio::ip::tcp::iostream s("127.0.0.1", port);
            if (!s.good()) {
                std::println(stderr, "can not connect");
                return -1;
            }
            s.put(Job::Cmd::SONG)
                .put(static_cast<uint8_t>(size))
                .write(sn.data(), size)
                .flush();
            if (!s.good()) {
                std::println(stderr, "can not send data");
                return -1;
            }
        }
        return 0;
    }

    int config(int const argc, char**const argv) noexcept {
        Args args(argc, argv, 1);
        if (!args.err.empty()) {
            std::println(stderr, "{}", args.err);
            return -2;
        }
        std::string port{args.port};
        if (port.empty()) {
            Config const conf(args);
            if (!conf.err.empty()) {
                std::println(stderr, "{}", conf.err);
                return -2;
            }
            port = std::format("{}", conf.daemon_port);
        }

        std::filesystem::path config;
        if (!args.config.empty())
            config = args.config;
        else {
            std::filesystem::path const cmd{args.cmd};
            if (cmd.has_parent_path())
                config = cmd.parent_path() / "config.toml";
            else config = "config.toml";
        }

        std::error_code ec;
        config = std::filesystem::absolute(config, ec);
        if (ec) {
            std::println(stderr, "fserr : {}", ec.message());
            return ec.value();
        }
        if (!std::filesystem::exists(config, ec)) {
            if (ec) {
                std::println(stderr, "fserr : {}", ec.message());
                return ec.value();
            }
            else {
                std::println(stderr, "not exist: {}", config.c_str());
                return -2;
            }
        }

        std::string_view const conf_name{config.c_str()};
        auto const size = conf_name.size();
        if (size >= 0xFF) {
            std::println(stderr, "config path too long (>255 char): {}", conf_name);
            return -1;
        }
        std::println("change config file: {}", conf_name);

        boost::asio::ip::tcp::iostream s("127.0.0.1", port);
        if (!s.good()) {
            std::println(stderr, "can not connect");
            return -1;
        }
        s.put(Job::Cmd::CONFIG)
            .put(static_cast<uint8_t>(size))
            .write(conf_name.data(), size)
            .flush();
        if (!s.good()) {
            std::println(stderr, "can not send data");
            return -1;
        }
        return 0;
    }
}