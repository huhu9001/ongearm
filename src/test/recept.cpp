#include"../src/wdmltd/recept.hpp"

#include<chrono>
#include<thread>
#include<print>

int main() {
    auto const kbd = "/dev/input/by-id/usb-Logitech_USB_Keyboard-event-kbd";
    Recept rpt(43212, kbd);
    if (!std::filesystem::exists(kbd)) {
        std::println("no kbd");
        return -1;
    }
    std::jthread th(+[]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        boost::asio::ip::tcp::iostream s("127.0.0.1", "43212");
        s.put(Recept::Command::RESUME).put(0).flush();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        s.put(Recept::Command::SONG).put(2).put('.').put('.').flush();
        s.close();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        s.connect("127.0.0.1", "43212");
        s.put(Recept::Command::QUIT).put(0);
        s.flush();
    });
    for (;;) {
        auto const r = rpt.poll();
        if (std::holds_alternative<input_event>(r)) {
            auto&ev = std::get<input_event>(r);
            std::println("key {}, {}", ev.code, ev.value);
        }
        else if (std::holds_alternative<Recept::Command>(r)) {
            auto&cmd = std::get<Recept::Command>(r);
            std::println("cmd {}, [{}]", (unsigned)cmd.kind, cmd.data.size());
            if (cmd.kind == Recept::Command::QUIT) return 0;
        }
        else if (std::holds_alternative<boost::system::error_code>(r)) {
            auto&err = std::get<boost::system::error_code>(r);
            std::println("err {}", err.message());
            return err.value();
        }
    }
}