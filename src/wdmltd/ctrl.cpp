#include"ctrl.hpp"

#include<cstring>

static uint16_t get_sort_key(Ctrls::Ctrl const&c) noexcept { return c.key; }

void Ctrls::add(Ctrl ctrl) noexcept {
    auto i = std::ranges::lower_bound(ctrls, get_sort_key(ctrl), {}, get_sort_key);
    ctrls.insert(i, ctrl);
}

void Ctrls::assign(std::vector<Ctrl>&&v) noexcept {
    ctrls.clear();
    ctrls.swap(v);
    std::ranges::sort(ctrls, {}, get_sort_key);
}

std::span<uint8_t const> Ctrls::input(uint16_t key, int32_t status) noexcept {
    if (status != 1) return {};
    auto const c = std::ranges::lower_bound(ctrls, key, {}, get_sort_key);
    if (c != ctrls.end() && c->key == key) {
        if (output.size() < 12) output.resize(12);
        auto const i = output.data();
        i[0] = PhantomInput::CMD_TOUCH_DOWN;
        i[1] = 0;
        std::memcpy(i + 2, &c->x, 4);
        std::memcpy(i + 6, &c->y, 4);
        i[10] = PhantomInput::CMD_TOUCH_UP;
        i[11] = 0;
        
        return {i, 12};output.emplace_back();
    }
    else return {};
}