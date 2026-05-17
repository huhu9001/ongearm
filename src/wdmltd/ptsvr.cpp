#include"ptsvr.hpp"

#include<cstdio>
#include<cstdlib>
#include<chrono>
#include<format>
#include<filesystem>
#include<fstream>
#include<print>

Ptsvr Ptsvr::connect(
    boost::asio::ip::tcp::iostream&socket,
    std::string_view ip,
    std::string_view const port,
    std::string_view const jar_path) noexcept {
    std::string ip_detected;
    if (ip.empty() || ip == "auto") {
        std::system("waydroid status|grep 'IP address'|cut -f 2 > /tmp/ongearm_waydroid_config");
        std::ifstream{"/tmp/ongearm_waydroid_config"} >> ip_detected;
        std::filesystem::remove("/tmp/ongearm_waydroid_config");
        if (ip_detected.empty()) {
            std::println(stderr, "unable to retrieve Waydroid ip");
            return {};
        }
        ip = ip_detected;
    }

    socket.connect(ip, port);
    if (socket) {
        std::println("connected to already running PhantomServer");
        return {};
    }
    if (jar_path.empty()) return {};

    std::thread th([jar_path, port]() noexcept {
        std::system(std::format(
            "echo CLASSPATH={} app_process /data/local/tmp"
            " com.phantom.server.PhantomServer -- --port {}"
            "|waydroid shell sh", jar_path, port).c_str());
    });
    Ptsvr ptsvr;
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
            ptsvr.pid = pid.substr(i0, i1 - i0);
            ptsvr.th = static_cast<std::thread&&>(th);
            std::println("PhantomServer started, pid {}", ptsvr.pid);
            break;
        }
        retry -= 1;
        if (retry > 0)
            std::this_thread::sleep_for(std::chrono::seconds{1});
        else {
            std::println(stderr, "unable to confirm PhantomServer process");
            th.detach();
            return {};
        }
    }
    for (auto retry = 5;;) {
        socket.clear();
        socket.connect(ip, port);
        if (socket) break;
        retry -= 1;
        if (retry > 0)
            std::this_thread::sleep_for(std::chrono::seconds{1});
        else break;
    }
    return ptsvr;
}

Ptsvr::~Ptsvr() {
    if (!th.joinable()) return;
    std::println("stop PhantomServer");
    std::system(std::format("waydroid shell kill -- {}", pid).c_str());
    th.join();
}