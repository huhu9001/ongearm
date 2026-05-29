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
            return cli::daemon(argc - 2, argv + 2);
        else if (cmd == "quit")
            return cli::cmds(argc - 2, argv + 2, Recept::Command::QUIT);
        else if (cmd == "pause")
            return cli::cmds(argc - 2, argv + 2, Recept::Command::PAUSE);
        else if (cmd == "resume")
            return cli::cmds(argc - 2, argv + 2, Recept::Command::RESUME);
        else if (cmd == "song")
            return cli::song(argc - 2, argv + 2);
        else if (cmd == "config")
            return cli::config(argc - 2, argv + 2);
        
        std::println(stderr, "bad command: {}", cmd);
        return -2;
    }
<<<<<<< HEAD
    else std::println("wdmltd v0.1");
}
=======
    else std::println("wdmltd v0.2");
}
>>>>>>> master
