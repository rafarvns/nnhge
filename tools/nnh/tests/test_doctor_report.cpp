// nnh doctor's report, verdicts and exit codes (doctor/doctor_report.hpp, spec 033 FR-20, FR-21).

#include "doctor/doctor_report.hpp"
#include "test.hpp"

#include <string>

using namespace nnh::tool;

namespace {

DoctorReport sample() {
    DoctorReport r;
    r.groups.push_back({"About", false, "", {{CheckState::Ok, "nnh", "nnh 0.1.0 (...)", ""}}});
    r.groups[0].verdict = false; // information only: no verdict line
    r.groups.push_back(
        {"Simulator",
         true,
         "",
         {{CheckState::Ok, "cmake", "4.4.2 (C:/cmake.exe)", ""},
          {CheckState::Fail, "ninja", "not found on PATH", "winget install Ninja-build.Ninja"}}});
    r.groups.push_back(
        {"3DS",
         true,
         "",
         {{CheckState::Fail, "devkitPro", "not found", "https://devkitpro.org/wiki/Getting_Started"},
          {CheckState::Fail, "libctru", "missing", "pacman -S 3ds-dev"},
          {CheckState::Ok, "citro2d", "1.7.0", ""}}});
    r.groups.push_back(
        {"Optional", false, "", {{CheckState::Warn, "ccache", "not found", "winget install ccache"}}});
    r.groups.push_back({"Console", true, "--console <ip> or --online", {}});
    r.groups.push_back(
        {"This project", true, "", {{CheckState::Warn, "engine", "0.2.0 (nnh 0.1.0)", "use nnh 0.2.0"}}});
    return r;
}

bool has(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

} // namespace

NNH_TEST(each_check_is_one_line_with_a_fix_under_every_problem) {
    const std::string text = format_report(sample(), false, false);
    CHECK(has(text, "About\n  ok    nnh              nnh 0.1.0 (...)\n"));
    CHECK(has(text, "Simulator\n  ok    cmake            4.4.2 (C:/cmake.exe)\n"));
    CHECK(has(text,
              "  FAIL  ninja            not found on PATH\n        fix: winget install Ninja-build.Ninja\n"));
    CHECK(has(text, "  warn  ccache           not found\n        fix: winget install ccache\n"));
    CHECK(!has(text, "Console\n")); // a group not checked has only its verdict line
    CHECK(!has(text, "\x1b["));     // no colour asked: plain text
}

NNH_TEST(long_names_still_leave_a_space) {
    DoctorReport r;
    r.groups.push_back({"G", true, "", {{CheckState::Ok, "a-very-long-check-name", "x", ""}}});
    CHECK(has(format_report(r, false, false), "  ok    a-very-long-check-name x\n"));
}

NNH_TEST(verdict_lines_count_problems_and_warnings_per_group) {
    const auto lines = verdict_lines(sample(), false);
    CHECK(lines.size() == 5);
    if (lines.size() != 5) return;
    CHECK(lines[0] == "Simulator: not ready (1 problem)");
    CHECK(lines[1] == "3DS: not ready (2 problems)");
    CHECK(lines[2] == "Optional: 1 warning");
    CHECK(lines[3] == "Console: not checked (--console <ip> or --online)");
    CHECK(lines[4] == "This project: ready (1 warning)");
    const std::string text = format_report(sample(), false, false);
    CHECK(has(text, "\nSimulator: not ready (1 problem)\n3DS: not ready (2 problems)\n"));
}

NNH_TEST(pc_only_shows_the_3ds_problems_without_counting_them) {
    const auto lines = verdict_lines(sample(), true);
    CHECK(lines.size() == 5 && lines[1] == "3DS: not ready (2 problems; not counted: --pc)");
    CHECK(has(format_report(sample(), false, true), "  FAIL  devkitPro"));
}

NNH_TEST(the_exit_code_counts_only_failures_of_required_groups) {
    DoctorReport r = sample();
    CHECK(doctor_exit_code(r, false) == 1);
    CHECK(doctor_exit_code(r, true) == 1); // the Simulator still fails
    r.groups[1].checks[1].state = CheckState::Ok;
    CHECK(doctor_exit_code(r, false) == 1); // the 3DS fails
    CHECK(doctor_exit_code(r, true) == 0);  // ... but --pc doesn't count it
    r.groups[2].checks[0].state = CheckState::Warn;
    r.groups[2].checks[1].state = CheckState::Ok;
    CHECK(doctor_exit_code(r, false) == 0);         // warnings never count
    r.groups[0].checks[0].state = CheckState::Fail; // About and Optional are not required
    r.groups[3].checks[0].state = CheckState::Fail;
    CHECK(doctor_exit_code(r, false) == 0);
    CHECK(verdict_lines(r, false)[2] == "Optional: 1 problem (not required)");
}

NNH_TEST(colours_wrap_the_state_words_only) {
    const std::string text = format_report(sample(), true, false);
    CHECK(has(text, "\x1b[32mok\x1b[0m    cmake"));
    CHECK(has(text, "\x1b[31mFAIL\x1b[0m  ninja"));
    CHECK(has(text, "\x1b[33mwarn\x1b[0m  ccache"));
    CHECK(has(text, "        fix: winget install Ninja-build.Ninja\n"));
}
