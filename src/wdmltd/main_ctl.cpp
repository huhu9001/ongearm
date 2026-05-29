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
        Config const conf(argc, argv);
        if (!conf.good) return -2;
        auto const port = std::format("{}", conf.daemon_port);
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
        if (argc <= 0) {
            std::println(stderr, "song name not given");
            return -2;
        }

        Config const conf(argc - 1, argv + 1);
        if (!conf.good) return -2;
        auto const port = std::format("{}", conf.daemon_port);

        std::string song_name{argv[0]};
        if (song_name.contains('*')) {
            auto const head = std::string_view{song_name}.substr(0, song_name.find('*'));
            auto const tail = std::string_view{song_name}.substr(song_name.rfind('*') + 1);
            std::vector<std::string> candi;
            std::error_code ec;
            std::filesystem::directory_iterator dir(conf.song_dir, ec);
            if (ec) {
                std::println(stderr, "error: {}", ec.message());
                return ec.value();
            }
            decltype(dir) const end;
            while (dir != end) {
                auto const s = dir->path().stem().string();
                if (s.starts_with(head) && s.ends_with(tail))
                    candi.push_back(dir->path().filename().string());
                dir.increment(ec);
                if (ec) {
                    std::println(stderr, "error: {}", ec.message());
                    return ec.value();
                }
            }

            if (candi.empty()) {
                std::println(stderr, "no such song");
                return -2;
            }
            else if (candi.size() == 1) {
                song_name = std::move(candi.front());
            }
            else {
                for (size_t n = 0; auto&s : candi) {
                    std::println("{}. {}", n, s);
                    n += 1;
                }
                std::print("select:");
                size_t n;
                std::cin >> n;
                if (n >= candi.size()) {
                    std::println(stderr, "invalid selection");
                    return -2;
                }
                song_name = std::move(candi[n]);
            }
        }
        std::println("new song: {}", song_name);

        auto const size = song_name.size();
        if (size >= 0xFF) {
            std::println(stderr, "song path too long (>255 char): {}", song_name);
            return -1;
        }

        boost::asio::ip::tcp::iostream s("127.0.0.1", port);
        if (!s.good()) {
            std::println(stderr, "can not connect");
            return -1;
        }
        s.put(Recept::Command::SONG)
            .put(static_cast<uint8_t>(size))
            .write(song_name.data(), size)
            .flush();
        if (!s.good()) {
            std::println(stderr, "can not send data");
            return -1;
        }
        return 0;
    }

    int config(int const argc, char**const argv) noexcept {
        std::string_view const conf_name = argc > 0 ? argv[0] : "config.toml";

        std::error_code ec;
        auto const p = std::filesystem::absolute(conf_name, ec);
        if (ec) {
            std::println(stderr, "error : {}", ec.message());
            return ec.value();
        }
        auto const p_str = p.string();
        auto const size = p_str.size();
        if (size >= 0xFF) {
            std::println(stderr, "config path too long (>255 char): {}", p_str);
            return -1;
        }
        std::println("new config file: {}", p_str);

        auto const conf =
            argc > 0 ? Config(argc - 1, argv + 1) : Config(0, nullptr);
        if (!conf.good) return -2;
        auto const port = std::format("{}", conf.daemon_port);

        boost::asio::ip::tcp::iostream s("127.0.0.1", port);
        if (!s.good()) {
            std::println(stderr, "can not connect");
            return -1;
        }
        s.put(Recept::Command::CONFIG)
            .put(static_cast<uint8_t>(size))
            .write(p_str.data(), size)
            .flush();
        if (!s.good()) {
            std::println(stderr, "can not send data");
            return -1;
        }
        return 0;
    }
}