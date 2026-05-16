#include<parser.hpp>
#include"debug.hpp"

#include<iostream>
#include<sstream>

int main() {
    using namespace ongearm;

    [[maybe_unused]] static constexpr char const*sample_input_0 =
        "0:00:00.500,0!\n"
        "0:00:01.000,1U3+\n"
        "0:00:02.000,4-\n";

    [[maybe_unused]] static constexpr ScrnMapper sample_mapper{
        .left = 200,
        .right = 500,
        .y = 700,
        .touch_slop = 40,
        .orient = ScrnMapper::Orient::R0,
        .num = 4,
    };

    std::vector<std::string> warnings;

    std::istringstream csv{sample_input_0};
    auto notes = parse_note(csv, &warnings);
    std::cout << "===notes===\n";
    for (auto n : notes) {
        std::cout << std::format("{}\n", n);
    }
    std::cout << "===warnings===\n";
    for (auto const&w : warnings) {
        std::cout << std::format("{}\n", w);
    }
    if (warnings.size() != 1) return -1;
    warnings.clear();

    auto motions = note_to_mo(notes, sample_mapper, &warnings);
    std::cout << "===motions===\n";
    for (auto m : motions) {
        std::cout << std::format("{}\n", m);
    }
    std::cout << "===warnings===\n";
    for (auto const&w : warnings) {
        std::cout << std::format("{}\n", w);
    }
    if (!warnings.empty()) return -1;
    warnings.clear();
}