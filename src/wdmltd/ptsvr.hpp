#pragma once

#include<boost/asio.hpp>

#include<string>
#include<string_view>
#include<thread>

struct Ptsvr {
    static Ptsvr connect(
        boost::asio::ip::tcp::iostream&socket,
        std::string_view ip,
        std::string_view port,
        std::string_view jar_path) noexcept;
    Ptsvr(Ptsvr&&) noexcept = default;
    ~Ptsvr();
private:
    Ptsvr() noexcept = default;
    std::string pid;
    std::thread th;
};