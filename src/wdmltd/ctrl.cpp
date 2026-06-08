#include"ctrl.hpp"

#include<cstring>
#include<cmath>
#include<utility>

static uint16_t get_sort_key(CtrlPanel::KeyIndex const&ki) noexcept { return ki.key; }

struct P {
    int32_t x, y;
};

static P dpad_dst(CtrlPanel::DPadCtrl const&d) noexcept {
    return {
        d.x + (d.right ? d.r : 0) - (d.left ? d.r : 0),
        d.y + (d.down ? d.r : 0) - (d.up ? d.r : 0),
    };
}

static void do_dpad(
        CtrlPanel::DPadCtrl&d,
        int32_t const x0,
        int32_t const y0,
        std::deque<std::vector<uint8_t>>&out) noexcept {
    constexpr auto STEP = 20;
    auto const[x_dst, y_dst] = dpad_dst(d);
    auto const diff_x = std::abs(x_dst - x0);
    auto const diff_y = std::abs(y_dst - y0);
    auto const n =
        static_cast<size_t>(std::max((diff_x + STEP - 1) / STEP, (diff_y + STEP - 1) / STEP));
    if (n > 0) {
        auto&o = out.emplace_back();
        o.resize(n * 10);
        for (size_t i = 0; i < n; ++i) {
            o[i * 10] = PhantomInput::CMD_TOUCH_MOVE;
            o[i * 10 + 1] = d.slot;
            int32_t x, y;
            auto const mul = static_cast<int32_t>(i + 1);
            if (x_dst > x0) {
                auto const x_try = x0 + mul * STEP;
                x = x_try >= x_dst ? x_dst : x_try;
            }
            else {
                auto const x_try = x0 - mul * STEP;
                x = x_try <= x_dst ? x_dst : x_try;
            }
            if (y_dst > y0) {
                auto const y_try = y0 + mul * STEP;
                y = y_try >= y_dst ? y_dst : y_try;
            }
            else {
                auto const y_try = y0 - mul * STEP;
                y = y_try <= y_dst ? y_dst : y_try;
            }
            std::memcpy(o.data() + i * 10 + 2, &x, 4);
            std::memcpy(o.data() + i * 10 + 6, &y, 4);
        }
    }
}

size_t CtrlPanel::assign(std::span<CtrlPanel::Ctrl const>const ctrls) noexcept {
    indices.clear();
    taps.clear();
    dpads.clear();
    swipes.clear();
    auto n = ctrls.size();
    for (auto&c : ctrls) {
        switch (c.type) {
        default: n -= 1; break;
        case Ctrl::PAUSE:
            indices.push_back({
                .key = c.key,
                .type = KeyIndex::PAUSE,
                .ctrl = c.ctrl,
                .alt = c.alt,
                .shift = c.shift,
            });
            break;
        case Ctrl::SONG:
            indices.push_back({
                .key = c.key,
                .type = KeyIndex::SONG,
                .ctrl = c.ctrl,
                .alt = c.alt,
                .shift = c.shift,
            });
            break;
        case Ctrl::TAP:
            indices.push_back({
                .idx = taps.size(),
                .key = c.tap.key,
                .type = KeyIndex::TAP,
                .ctrl = c.ctrl,
                .alt = c.alt,
                .shift = c.shift,
            });
            taps.push_back({
                .x = c.tap.x,
                .y = c.tap.y,
                .slot = SLOT_NUM,
            });
            break;
        case Ctrl::DPAD:
            indices.push_back({
                .idx = dpads.size(),
                .key = c.dpad.up,
                .type = KeyIndex::DPAD_UP,
                .ctrl = c.ctrl,
                .alt = c.alt,
                .shift = c.shift,
            });
            indices.push_back({
                .idx = dpads.size(),
                .key = c.dpad.down,
                .type = KeyIndex::DPAD_DOWN,
                .ctrl = c.ctrl,
                .alt = c.alt,
                .shift = c.shift,
            });
            indices.push_back({
                .idx = dpads.size(),
                .key = c.dpad.left,
                .type = KeyIndex::DPAD_LEFT,
                .ctrl = c.ctrl,
                .alt = c.alt,
                .shift = c.shift,
            });
            indices.push_back({
                .idx = dpads.size(),
                .key = c.dpad.right,
                .type = KeyIndex::DPAD_RIGHT,
                .ctrl = c.ctrl,
                .alt = c.alt,
                .shift = c.shift,
            });
            dpads.push_back({
                .x = c.dpad.x,
                .y = c.dpad.y,
                .r = c.dpad.r,
                .slot = SLOT_NUM,
                .up = false,
                .down = false,
                .left = false,
                .right = false,
            });
            break;
        case Ctrl::SWIPE: {
            auto const t = c.swipe.t;
            indices.push_back({
                .idx = swipes.size(),
                .key = c.swipe.key,
                .type = KeyIndex::SWIPE,
                .ctrl = c.ctrl,
                .alt = c.alt,
                .shift = c.shift,
            });
            auto const n =
                c.swipe.t.count() / SwipeCtrl::INTERVAL.count();
            int32_t const dx =
                n > 0 ? (c.swipe.x_to - c.swipe.x_from) / n : 0;
            int32_t const dy =
                n > 0 ? (c.swipe.y_to - c.swipe.y_from) / n : 0;
            swipes.push_back({
                .timer{ctx},
                .output = output,
                .t = c.swipe.t,
                .x_from = c.swipe.x_from,
                .y_from = c.swipe.y_from,
                .x_to = c.swipe.x_to,
                .y_to = c.swipe.y_to,
                .dx = dx,
                .dy = dy,
                .slot = SLOT_NUM,
            });
        } break;
        }
    }
    std::ranges::sort(indices, {}, get_sort_key);

    auto&o = output.emplace_back();
    o.push_back(PhantomInput::CMD_TOUCH_CANCEL);

    return n;
}

void CtrlPanel::input(uint16_t key, int32_t status) noexcept {
    if (status == 1) {
        switch (key) {
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL: ctrl = true; break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT: alt = true; break;
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT: shift = true; break;
        }

        for (auto c = std::ranges::lower_bound(
            indices, key, {}, get_sort_key); c != indices.end() && c->key == key; ++c)
        if (c->ctrl == ctrl && c->alt == alt && c->shift == shift) {
            switch (c->type) {
            case KeyIndex::PAUSE: pause = true; break;
            case KeyIndex::SONG: song = true; break;
            case KeyIndex::TAP: {
                auto&tap = taps[c->idx];
                if (tap.slot == SLOT_NUM) {
                    if (auto const slot = get_slot(); slot != SLOT_NUM) {
                        tap.slot = slot;
                        busy[slot] = true;
                        auto&o = output.emplace_back();
                        o.resize(10);
                        o[0] = PhantomInput::CMD_TOUCH_DOWN;
                        o[1] = slot;
                        std::memcpy(o.data() + 2, &tap.x, 4);
                        std::memcpy(o.data() + 6, &tap.y, 4);
                    }
                }
            } break;
            case KeyIndex::DPAD_UP:
            case KeyIndex::DPAD_DOWN:
            case KeyIndex::DPAD_LEFT:
            case KeyIndex::DPAD_RIGHT: {
                auto&d = dpads[c->idx];
                if (d.slot == SLOT_NUM) {
                    if (auto const slot = get_slot(); slot != SLOT_NUM) {
                        d.slot = slot;
                        busy[slot] = true;

                        d.up = c->type == KeyIndex::DPAD_UP;
                        d.down = c->type == KeyIndex::DPAD_DOWN;
                        d.left = c->type == KeyIndex::DPAD_LEFT;
                        d.right = c->type == KeyIndex::DPAD_RIGHT;

                        auto const[x_dst, y_dst] = dpad_dst(d);
                        auto&o = output.emplace_back();
                        o.resize(20);
                        o[0] = PhantomInput::CMD_TOUCH_DOWN;
                        o[1] = slot;
                        std::memcpy(o.data() + 2, &x_dst, 4);
                        std::memcpy(o.data() + 6, &y_dst, 4);
                        o[10] = PhantomInput::CMD_TOUCH_MOVE;
                        o[11] = slot;
                        std::memcpy(o.data() + 12, &x_dst, 4);
                        std::memcpy(o.data() + 16, &y_dst, 4);
                    }
                }
                else {
                    auto const[x0, y0] = dpad_dst(d);
                    d.up |= c->type == KeyIndex::DPAD_UP;
                    d.down |= c->type == KeyIndex::DPAD_DOWN;
                    d.left |= c->type == KeyIndex::DPAD_LEFT;
                    d.right |= c->type == KeyIndex::DPAD_RIGHT;
                    do_dpad(d, x0, y0, output);
                }
            } break;
            case KeyIndex::SWIPE: {
                auto&sw = swipes[c->idx];
                if (sw.slot == SLOT_NUM) {
                    if (auto const slot = get_slot(); slot != SLOT_NUM) {
                        sw.slot = slot;
                        busy[slot] = true;

                        auto&o = sw.output.emplace_back();
                        if (sw.t.count() > 0) {
                            o.resize(20);
                            o[0] = PhantomInput::CMD_TOUCH_DOWN;
                            o[1] = sw.slot;
                            std::memcpy(o.data() + 2, &sw.x_from, 4);
                            std::memcpy(o.data() + 6, &sw.y_from, 4);
                            o[10] = PhantomInput::CMD_TOUCH_MOVE;
                            o[11] = sw.slot;
                            std::memcpy(o.data() + 12, &sw.x_from, 4);
                            std::memcpy(o.data() + 16, &sw.y_from, 4);

                            auto const n = sw.t.count() / SwipeCtrl::INTERVAL.count();
                            auto const t = sw.t.count() % SwipeCtrl::INTERVAL.count();
                            if (t > 0) {
                                sw.step = n;
                                sw.timer.expires_after(std::chrono::milliseconds{t});
                            }
                            else {
                                sw.step = n - 1;
                                sw.timer.expires_after(SwipeCtrl::INTERVAL);
                            }
                            sw.timer.async_wait(SwipeCtrl::Handler{sw});
                        }
                        else {
                            o.resize(30);
                            o[0] = PhantomInput::CMD_TOUCH_DOWN;
                            o[1] = sw.slot;
                            std::memcpy(o.data() + 2, &sw.x_from, 4);
                            std::memcpy(o.data() + 6, &sw.y_from, 4);
                            o[10] = PhantomInput::CMD_TOUCH_MOVE;
                            o[11] = sw.slot;
                            std::memcpy(o.data() + 12, &sw.x_from, 4);
                            std::memcpy(o.data() + 16, &sw.y_from, 4);
                            o[20] = PhantomInput::CMD_TOUCH_MOVE;
                            o[21] = sw.slot;
                            std::memcpy(o.data() + 22, &sw.x_to, 4);
                            std::memcpy(o.data() + 26, &sw.y_to, 4);
                        }
                    }
                }
            } break;
            }
        }
    }
    else if (status == 0) {
        switch (key) {
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL: ctrl = false; break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT: alt = false; break;
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT: shift = false; break;
        }

        for (auto c = std::ranges::lower_bound(
            indices, key, {}, get_sort_key); c != indices.end() && c->key == key; ++c)
        if (c->ctrl == ctrl && c->alt == alt && c->shift == shift) {
            switch (c->type) {
            case KeyIndex::TAP: {
                auto&tap = taps[c->idx];
                if (tap.slot < SLOT_NUM) {
                    auto&o = output.emplace_back();
                    o.push_back(PhantomInput::CMD_TOUCH_UP);
                    o.push_back(tap.slot);
                    busy[tap.slot] = false;
                    tap.slot = SLOT_NUM;
                }
            } break;
            case KeyIndex::DPAD_UP:
            case KeyIndex::DPAD_DOWN:
            case KeyIndex::DPAD_LEFT:
            case KeyIndex::DPAD_RIGHT: {
                auto&d = dpads[c->idx];
                if (d.slot < SLOT_NUM) {
                    auto const[x0, y0] = dpad_dst(d);
                    d.up &= c->type != KeyIndex::DPAD_UP;
                    d.down &= c->type != KeyIndex::DPAD_DOWN;
                    d.left &= c->type != KeyIndex::DPAD_LEFT;
                    d.right &= c->type != KeyIndex::DPAD_RIGHT;
                    if (d.up || d.down || d.left || d.right) {
                        do_dpad(d, x0, y0, output);
                    }
                    else {
                        auto&o = output.emplace_back();
                        o.resize(2);
                        o[0] = PhantomInput::CMD_TOUCH_UP;
                        o[1] = d.slot;
                        busy[d.slot] = false;
                        d.slot = SLOT_NUM;
                    }
                }
            } break;
            case KeyIndex::SWIPE: {
                auto&sw = swipes[c->idx];
                if (sw.slot < SLOT_NUM) {
                    sw.timer.cancel();
                    auto&o = output.emplace_back();
                    o.push_back(PhantomInput::CMD_TOUCH_UP);
                    o.push_back(sw.slot);
                    busy[sw.slot] = false;
                    sw.slot = SLOT_NUM;
                }
            } break;
            }
        }
    }
}

void CtrlPanel::poll(Job&j) noexcept {
    if (pause) {
        pause = false;
        j.kind = Job::PAUSE;
        return;
    }

    if (song) {
        song = false;
        j.kind = Job::SONG;
        return;
    }

    if (!output.empty()) {
        j.kind = Job::MOTION;
        j.mdata = std::move(output.front());
        output.pop_front();
        return;
    }

    j.kind = Job::NONE;
}

void CtrlPanel::SwipeCtrl::Handler::operator()(boost::system::error_code const&e) noexcept  {
    if (e) return;
    auto&o = sw.output.emplace_back();
    o.resize(10);
    o[0] = PhantomInput::CMD_TOUCH_MOVE;
    o[1] = sw.slot;
    const auto x = static_cast<int32_t>(sw.x_to - sw.step * sw.dx);
    const auto y = static_cast<int32_t>(sw.y_to - sw.step * sw.dy);
    std::memcpy(o.data() + 2, &x, 4);
    std::memcpy(o.data() + 6, &y, 4);
    if (sw.step > 0) {
        sw.step -= 1;
        sw.timer.expires_after(INTERVAL);
        sw.timer.async_wait(Handler{sw});
    }
}
