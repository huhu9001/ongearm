#pragma once

#include<boost/asio.hpp>
#include<boost/system.hpp>

extern "C" {
#include<linux/input.h>
}

#include<cstdint>
#include<cstddef>
#include<condition_variable>
#include<deque>
#include<filesystem>
#include<future>
#include<mutex>
#include<new>
#include<string_view>
#include<system_error>
#include<thread>
#include<utility>
#include<variant>
#include<vector>

struct Recept {
    struct Command {
        std::vector<uint8_t> data;
        enum {
            QUIT,
            PAUSE,
            RESUME,
            CONFIG,
            SONG,
        };
        uint8_t kind;
    };
    typedef boost::system::error_code Err;
    typedef input_event Input;

    Recept(int port, std::filesystem::path kbd_name) noexcept:
        ac(ctx),
        sc(ctx),
        kbd(ctx) {
        Err ec;
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        ac.open(endpoint.protocol(), ec);
        if (ec) ec_ac = ec;
        ac.bind(endpoint, ec);
        if (ec) ec_ac = ec;
        ac.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) ec_ac = ec;
        kbd.open(kbd_name, boost::asio::file_base::flags::read_only, ec);
        if (ec) ec_kbd = ec;

        ac.async_accept(sc, on_accept);
        boost::asio::async_read(
            kbd, boost::asio::mutable_buffer(&ev, sizeof(Input)), on_kbd);
    }

    std::variant<Input, Command, Err> poll() noexcept {
        for (;;) {
            if (ec_ac) return ec_ac;
            if (ec_kbd) return ec_kbd;

            if (!data_cmd.empty()) {
                auto cmd = std::move(data_cmd.front());
                data_cmd.pop_front();
                return cmd;
            }
            if (!data_input.empty()) {
                auto ev = data_input.front();
                data_input.pop_front();
                return ev;
            }

            ctx.run_one();
            ctx.restart();
        }
    }

private:
    boost::asio::io_context ctx;
    boost::asio::ip::tcp::acceptor ac;
    boost::asio::ip::tcp::socket sc;
    boost::asio::stream_file kbd;

    Input ev;
    uint8_t cmd_head[2];
    std::vector<uint8_t> buf_cmd;
    Err ec_kbd;
    Err ec_ac;

    std::deque<Input> data_input;
    std::deque<Command> data_cmd;

    struct {
        Recept&r;
        void operator()(Err const&ec) noexcept {
            if (ec) r.ec_ac = ec;
            else
                boost::asio::async_read(
                    r.sc, boost::asio::mutable_buffer(r.cmd_head, 2), r.on_cmd);
        }
    } on_accept{*this};

    struct {
        Recept&r;
        void operator()(Err const&ec, size_t n) noexcept {
            if (ec) {
                Err e;
                r.sc.close(e);
                r.ac.async_accept(r.sc, r.on_accept);
            }
            else if (size_t const size = r.cmd_head[1]; size > 0) {
                r.buf_cmd.resize(size);
                boost::asio::async_read(
                    r.sc, boost::asio::mutable_buffer(r.buf_cmd.data(), size), r.on_cmd_data);
            }
            else {
                r.data_cmd.emplace_back(decltype(Command::data){}, r.cmd_head[0]);
                boost::asio::async_read(
                    r.sc, boost::asio::mutable_buffer(r.cmd_head, 2), *this);
            }
        }
    } on_cmd{*this};

    struct {
        Recept&r;
        void operator()(Err const&ec, size_t n) noexcept {
            if (ec) {
                Err e;
                r.sc.close(e);
                r.ac.async_accept(r.sc, r.on_accept);
            }
            else {
                r.data_cmd.emplace_back(std::move(r.buf_cmd), r.cmd_head[0]);
                boost::asio::async_read(
                    r.sc, boost::asio::mutable_buffer(r.cmd_head, 2), r.on_cmd);
            }
        }
    } on_cmd_data{*this};

    struct {
        Recept&r;
        void operator()(Err const&ec, size_t n) noexcept {
            if (ec) r.ec_kbd = ec;
            else {
                if (r.ev.type == EV_KEY) r.data_input.push_back(r.ev);
                boost::asio::async_read(
                    r.kbd, boost::asio::mutable_buffer(&r.ev, sizeof(Input)), *this);
            }
        }
    } on_kbd{*this};
};