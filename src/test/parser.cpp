#include<parser.hpp>
#include"debug.hpp"

#include<iostream>
#include<sstream>

int main() {
    using namespace ongearm;
    std::vector<NoteEvent> notes;
    std::vector<MotionEvent> motions;
    std::vector<std::string> warnings;

    {
        std::istringstream l{"84078,1+\n0:00:85.078,2+"};
        parse_note(notes, l, nullptr);
        if (notes.size() != 2) return -1;
        if (notes[0].time != 84078) return -1;
        if (notes[1].time != 85078) return -1;
        notes.clear();
    }

    {
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

        std::istringstream csv{sample_input_0};
        parse_note(notes, csv, &warnings);
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

        note_to_mo(motions, notes, sample_mapper, &warnings);
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
        notes.clear();
        motions.clear();
    }
}