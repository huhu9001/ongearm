#include"recept.hpp"

#include<filesystem>
#include<string_view>
#include<print>

namespace cli {
    int daemon(int, char**) noexcept;
    int cmds(int, char**, uint8_t) noexcept;
    int song(int, char**) noexcept;
    int config(int, char**) noexcept;
}

int main(int const argc, char**const argv) {
    if (argc >= 2) {
        std::string_view const cmd{argv[1]};
        if (cmd == "run")
            return cli::daemon(argc, argv);
        else if (cmd == "quit")
            return cli::cmds(argc, argv, Job::Cmd::QUIT);
        else if (cmd == "pause")
            return cli::cmds(argc, argv, Job::Cmd::PAUSE);
        else if (cmd == "resume")
            return cli::cmds(argc, argv, Job::Cmd::RESUME);
        else if (cmd == "song")
            return cli::song(argc, argv);
        else if (cmd == "config")
            return cli::config(argc, argv);
        
        std::println(stderr, "bad command: {}", cmd);
        return -2;
    }
    else std::println("wdmltd v0.2");
}
