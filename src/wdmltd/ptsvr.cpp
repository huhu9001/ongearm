#include"ptsvr.hpp"

#include<cstdio>
#include<chrono>
#include<format>
#include<filesystem>
#include<fstream>
#include<print>
#include<string>
#include<system_error>
#include<utility>

static constexpr auto CMD_START =
    "echo CLASSPATH={} app_process /data/local/tmp com.phantom.server.PhantomServer -- --port {}|waydroid shell sh";

PtsvrConnection::PtsvrConnection(
    std::string_view const ip,
    std::string_view const port,
    std::string_view const jar_path) noexcept:port(port) {
    if (ip.empty() || ip == "auto") {
        std::system("waydroid status|grep 'IP address'|cut -f 2 > /tmp/ongearm_waydroid_config");
        std::ifstream{"/tmp/ongearm_waydroid_config"} >> this->ip;
        std::error_code ec;
        std::filesystem::remove("/tmp/ongearm_waydroid_config", ec);
        if (ec) {
            std::println(stderr, "temp file cleanup failed: {}", ec.message());
        }
        if (this->ip.empty()) {
            std::println(stderr, "unable to retrieve Waydroid ip");
            socket.setstate(std::ios_base::badbit);
            return;
        }
    }
    else this->ip = ip;

    socket.connect(std::string_view{this->ip}, port);
    if (socket) {
        std::println("connected to already running PhantomServer");
        return;
    }
    if (jar_path.empty()) return;

    std::promise<void> done_handle;
    done = done_handle.get_future();
    th = std::thread([](std::string const cmd, std::promise<void> done) noexcept {
        std::system(cmd.c_str());
        done.set_value();
    }, std::format(CMD_START, jar_path, port), std::move(done_handle));
    for (auto retry = 10;;) {
        socket.clear();
        socket.connect(std::string_view{this->ip}, port);
        if (socket) break;
        retry -= 1;
        if (retry > 0)
            std::this_thread::sleep_for(std::chrono::seconds{1});
        else break;
    }
}

PtsvrConnection::~PtsvrConnection() {
    if (!th.joinable()) return;
    if (done.wait_for(std::chrono::milliseconds{1}) == std::future_status::ready) {
        th.join();
        return;
    }
    if (retry()) {
        std::println("stop PhantomServer");
        socket.put(0x7E).flush(); // send an invalid byte to kill
    }
    if (done.wait_for(std::chrono::seconds{3}) == std::future_status::ready) {
        th.join();
        return;
    }
    std::println(stderr, "warning: PhantomServer process is lost");
    th.detach();
}

bool PtsvrConnection::close() noexcept {
    if (socket.eof()) return true;
    socket.close();
    if (socket.fail()) return false;
    socket.setstate(std::ios_base::eofbit);
    return true;
}

bool PtsvrConnection::retry() noexcept {
    if (socket.good()) return true;
    socket.clear();
    socket.connect(std::string_view{ip}, std::string_view{port});
    return socket.good();
}