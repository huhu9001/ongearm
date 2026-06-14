#pragma once

#include<boost/asio.hpp>

#include<future>
#include<string_view>
#include<thread>

struct PtsvrConnection {
    [[nodiscard("Phantom server closes when PtsvrConnection destroyed")]]
    PtsvrConnection(
        std::string_view ip,
        std::string_view port,
        std::string_view jar_path) noexcept;
    PtsvrConnection(PtsvrConnection&&) noexcept = default;
    ~PtsvrConnection();
    boost::asio::ip::tcp::iostream socket;
    bool close() noexcept;
    bool retry() noexcept;
private:
    std::string ip;
    std::string port;
    std::thread th;
    std::future<void> done;
};