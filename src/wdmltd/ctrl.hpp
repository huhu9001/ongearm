#pragma once

#include"phantom.hpp"

#include<cstddef>
#include<cstdint>
#include<algorithm>
#include<span>
#include<vector>

struct Ctrls {
    struct Ctrl {
        uint16_t key;
        int32_t x, y;
    };
    void add(Ctrl) noexcept;
    void assign(std::vector<Ctrl>) noexcept;
    std::span<uint8_t const> input(uint16_t key, int32_t status) noexcept;
private:
    std::vector<Ctrl> ctrls;
    std::vector<uint8_t> output;
};