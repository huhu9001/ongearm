#pragma once

#include"jobs.hpp"

#include<boost/asio.hpp>
#include<boost/system.hpp>

extern "C" {
#include<linux/input.h>
}

#include<cstdint>
#include<cstddef>
#include<deque>
#include<filesystem>
#include<string_view>
#include<utility>
#include<vector>

struct Recept {
    Recept(
        boost::asio::io_context&ctx,
        int port,
        std::filesystem::path const&kbd_name) noexcept:
        ctx(ctx),
        ac(ctx),
        sc(ctx),
        kbd(ctx) {
        boost::system::error_code ec;
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        ac.open(endpoint.protocol(), ec);
        if (ec) {
            ec_ac = ec;
            return;
        }
        ac.bind(endpoint, ec);
        if (ec) {
            ec_ac = ec;
            return;
        }
        ac.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) {
            ec_ac = ec;
            return;
        }
        kbd.open(kbd_name, boost::asio::file_base::flags::read_only, ec);
        if (ec) {
            ec_ac = ec;
            return;
        }

        ac.async_accept(sc, OnAccept{*this});
        boost::asio::async_read(
            kbd, boost::asio::mutable_buffer(&ev, sizeof(input_event)), OnKbd{*this});
    }
    Recept(Recept const&) = delete;
    Recept(Recept&&) = delete;

    void poll(Job&j) noexcept {
        if (ec_ac) {
            j.kind = Job::ERR;
            j.err = ec_ac.message();
            return;
        }
        if (ec_kbd) {
            j.kind = Job::ERR;
            j.err = ec_kbd.message();
            return;
        }

        if (!data_cmd.empty()) {
            j.kind = Job::CMD;
            j.cmd = std::move(data_cmd.front());
            data_cmd.pop_front();
            return;
        }
        if (!data_input.empty()) {
            j.kind = Job::KEY;
            j.ev = data_input.front();
            data_input.pop_front();
            return;
        }

        j.kind = Job::NONE;
    }

private:
    boost::asio::io_context&ctx;
    boost::asio::ip::tcp::acceptor ac;
    boost::asio::ip::tcp::socket sc;
    boost::asio::stream_file kbd;

    input_event ev;
    uint8_t cmd_head[2];
    std::vector<uint8_t> buf_cmd;
    boost::system::error_code ec_kbd;
    boost::system::error_code ec_ac;

    std::deque<input_event> data_input;
    std::deque<Job::Cmd> data_cmd;

    struct OnAccept {
        Recept&r;
        void operator()(boost::system::error_code const&ec) noexcept {
            if (ec) r.ec_ac = ec;
            else
                boost::asio::async_read(
                    r.sc, boost::asio::mutable_buffer(r.cmd_head, 2), OnCmd{r});
        }
    };

    struct OnCmd {
        Recept&r;
        void operator()(boost::system::error_code const&ec, size_t n) noexcept {
            if (ec) {
                boost::system::error_code e;
                r.sc.close(e);
                r.ac.async_accept(r.sc, OnAccept{r});
            }
            else if (size_t const size = r.cmd_head[1]; size > 0) {
                r.buf_cmd.resize(size);
                boost::asio::async_read(
                    r.sc, boost::asio::mutable_buffer(r.buf_cmd.data(), size), OnCmdData{r});
            }
            else {
                r.data_cmd.emplace_back(decltype(Job::Cmd::data){}, r.cmd_head[0]);
                boost::asio::async_read(
                    r.sc, boost::asio::mutable_buffer(r.cmd_head, 2), *this);
            }
        }
    };

    struct OnCmdData {
        Recept&r;
        void operator()(boost::system::error_code const&ec, size_t n) noexcept {
            if (ec) {
                boost::system::error_code e;
                r.sc.close(e);
                r.ac.async_accept(r.sc, OnAccept{r});
            }
            else {
                r.data_cmd.emplace_back(std::move(r.buf_cmd), r.cmd_head[0]);
                boost::asio::async_read(
                    r.sc, boost::asio::mutable_buffer(r.cmd_head, 2), OnCmd{r});
            }
        }
    };

    struct OnKbd {
        Recept&r;
        void operator()(boost::system::error_code const&ec, size_t n) noexcept {
            if (ec) r.ec_kbd = ec;
            else {
                if (r.ev.type == EV_KEY) r.data_input.push_back(r.ev);
                boost::asio::async_read(
                    r.kbd, boost::asio::mutable_buffer(&r.ev, sizeof(input_event)), *this);
            }
        }
    };
};
