#include<parser.hpp>
#include<debug.hpp>
#include<asserts.hpp>

#include<sstream>

int main() {
    using namespace ongearm;
    std::vector<NoteEvent> notes;
    std::vector<MotionEvent> motions;
    std::vector<std::string> warnings;

    {
        std::istringstream l{"84078,1+\n0:00:85.078,2+"};
        parse_note(notes, l, nullptr);
        assert_eq(notes.size(), (size_t)2, "{}", __LINE__);
        assert_eq(notes[0].time, 84078, "{}", __LINE__);
        assert_eq(notes[1].time, 85078, "{}", __LINE__);
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
        assert_eq(warnings.size(), (size_t)1, "{}", __LINE__);
        warnings.clear();

        note_to_mo(motions, notes, sample_mapper, &warnings);
        assert_eq(warnings.size(), (size_t)0, "{}", __LINE__);
        warnings.clear();
        notes.clear();
        motions.clear();
    }
}