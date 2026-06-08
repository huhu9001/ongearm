#include"ctrl.hpp"

#include<asserts.hpp>

#include<cstring>

int main() {
    boost::asio::io_context ctx;
    CtrlPanel ctrls{ctx};
    ctrls.assign({
        {
            .dpad = {
                .x = 0, .y = 0, .r = 50,
                .up = 0, .down = 1, .left = 2, .right = 3,
            },
            .type = CtrlPanel::Ctrl::DPAD,
            .ctrl = false,
            .alt = false,
            .shift = false,
        },
    });
    Job o;
    {
        ctrls.poll(o);
        assert_eq((int)o.kind, (int)Job::MOTION, "0");
        assert_eq(o.mdata[0], (uint8_t)PhantomInput::CMD_TOUCH_CANCEL, "{}", __LINE__);
    }
    ctrls.input(0, 1);
    {
        ctrls.poll(o);
        assert_eq((int)o.kind, (int)Job::MOTION, "2");
        assert_eq(o.mdata.size(), (size_t)20, "3");
        assert_eq(o.mdata[0], (uint8_t)PhantomInput::CMD_TOUCH_DOWN, "{}", __LINE__);
        assert_eq(o.mdata[10], (uint8_t)PhantomInput::CMD_TOUCH_MOVE, "{}", __LINE__);
        assert_eq(o.mdata[1], (uint8_t)0, "6");
        int32_t x, y;
        std::memcpy(&x, o.mdata.data() + 2, 4);
        std::memcpy(&y, o.mdata.data() + 6, 4);
        assert_eq(x, 0, "7");
        assert_eq(y, -50, "8");
    }
    ctrls.input(3, 1);
    {
        ctrls.poll(o);
        assert_eq((int)o.kind, (int)Job::MOTION, "{}", __LINE__);
        assert_eq(o.mdata.size() % 10, (size_t)0, "{}", __LINE__);
        assert_eq(o.mdata[0], (uint8_t)PhantomInput::CMD_TOUCH_MOVE, "{}", __LINE__);
        auto const n = o.mdata.size() / 10;
        assert_eq(o.mdata[10 * (n - 1)], (uint8_t)PhantomInput::CMD_TOUCH_MOVE, "{}", __LINE__);
        assert_eq(o.mdata[1], (uint8_t)0, "{}", __LINE__);
        int32_t x0, y0;
        std::memcpy(&x0, o.mdata.data() + 2, 4);
        std::memcpy(&y0, o.mdata.data() + 6, 4);
        int32_t x, y;
        std::memcpy(&x, o.mdata.data() + 2 + 10 * (n - 1), 4);
        std::memcpy(&y, o.mdata.data() + 6 + 10 * (n - 1), 4);
        assert_eq(x, 50, "{}", __LINE__);
        assert_eq(y, -50, "{}", __LINE__);
        asserts(x > x0, "{}, {} > {}", __LINE__, x, x0);
        assert_eq(y, y0, "{}", __LINE__);
    }
    ctrls.input(3, 1);
    {
        ctrls.poll(o);
        assert_eq((int)o.kind, (int)Job::NONE, "{}", __LINE__);
    }
    ctrls.input(0, 0);
    {
        ctrls.poll(o);
        assert_eq((int)o.kind, (int)Job::MOTION, "{}", __LINE__);
        assert_eq(o.mdata.size() % 10, (size_t)0, "{}", __LINE__);
        assert_eq(o.mdata[0], (uint8_t)PhantomInput::CMD_TOUCH_MOVE, "{}", __LINE__);
        auto const n = o.mdata.size() / 10;
        assert_eq(o.mdata[10 * (n - 1)], (uint8_t)PhantomInput::CMD_TOUCH_MOVE, "{}", __LINE__);
        assert_eq(o.mdata[1], (uint8_t)0, "{}", __LINE__);
        int32_t x0, y0;
        std::memcpy(&x0, o.mdata.data() + 2, 4);
        std::memcpy(&y0, o.mdata.data() + 6, 4);
        int32_t x, y;
        std::memcpy(&x, o.mdata.data() + 2 + 10 * (n - 1), 4);
        std::memcpy(&y, o.mdata.data() + 6 + 10 * (n - 1), 4);
        assert_eq(x, 50, "{}", __LINE__);
        assert_eq(y, 0, "{}", __LINE__);
        assert_eq(x, x0, "{}", __LINE__);
        asserts(y > y0, "{}, {} > {}", __LINE__, y, y0);
    }
    ctrls.input(3, 0);
    {
        ctrls.poll(o);
        assert_eq((int)o.kind, (int)Job::MOTION, "{}", __LINE__);
        assert_eq(o.mdata.size(), (size_t)2, "{}", __LINE__);
        assert_eq(o.mdata[0], (uint8_t)PhantomInput::CMD_TOUCH_UP, "{}", __LINE__);
        assert_eq(o.mdata[1], (uint8_t)0, "{}", __LINE__);
    }
}