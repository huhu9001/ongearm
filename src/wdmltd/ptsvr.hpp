#pragma once

#include<boost/asio.hpp>

#include<future>
#include<string_view>
#include<thread>

struct PtsvrConnection {
    [[nodiscard("Phantom server closes when PtsvrConnection destroyed")]]
    PtsvrConnection(
        boost::asio::ip::tcp::iostream&socket,
        std::string_view ip,
        std::string_view port,
        std::string_view jar_path) noexcept;
    PtsvrConnection(PtsvrConnection&&) noexcept = default;
    ~PtsvrConnection();
private:
    boost::asio::ip::tcp::iostream&socket;
    std::thread th;
    std::future<void> done;
};