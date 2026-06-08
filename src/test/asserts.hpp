#pragma once

#include<cstdlib>
#include<exception>
#include<print>

struct AssertionFailedException:std::exception {};

template<typename...Args>
void asserts(bool b, std::format_string<Args...> msg, Args&&...args) noexcept(false) {
    if (!b) {
        std::println(stderr, "assertion failed");
        std::println(stderr, msg, static_cast<Args&&>(args)...);
        throw AssertionFailedException{};
    }
}

template<typename T1, typename T2, typename...Args>
void assert_eq(T1&&t1, T2&&t2, std::format_string<Args...> msg, Args&&...args) noexcept(false) {
    if (!(t1 == t2)) {
        std::println(stderr, "assertion failed: {} == {}", t1, t2);
        std::println(stderr, msg, static_cast<Args&&>(args)...);
        throw AssertionFailedException{};
    }
}

template<typename T1, typename T2, typename...Args>
void assert_ne(T1&&t1, T2&&t2, std::format_string<Args...> msg, Args&&...args) noexcept(false) {
    if (!(t1 != t2)) {
        std::println(stderr, "assertion failed: {} != {}", t1, t2);
        std::println(stderr, msg, static_cast<Args&&>(args)...);
        throw AssertionFailedException{};
    }
}