#pragma once
// nnh doctor's report (spec 033 FR-20, FR-21, D56 item 5): groups of checks, each ok / warn / FAIL with what
// was found and, for every warn and FAIL, one `fix:` line; then one verdict line per group; the exit code.
// Pure; tested in tools/nnh/tests/test_doctor_report.cpp.

#include <string>
#include <vector>

namespace nnh::tool {

enum class CheckState { Ok, Warn, Fail };

struct DoctorCheck {
    CheckState state = CheckState::Ok;
    std::string name;  ///< "cmake", "devkitPro", ...
    std::string found; ///< what was found: a version and a path, or what is missing
    std::string fix;   ///< one command or one link; required for warn and FAIL
};

struct DoctorGroup {
    std::string name;        ///< "Simulator", "3DS", ...
    bool required = true;    ///< a FAIL here makes the exit code 1 (Optional and About are not required)
    std::string not_checked; ///< non-empty: the group was skipped and this says why ("--online")
    std::vector<DoctorCheck> checks;
    bool verdict = true; ///< false: no verdict line (About: information only)
};

struct DoctorReport {
    std::vector<DoctorGroup> groups;
};

/// The checks, group by group ("  ok    cmake          4.4.2 (C:/...)", "        fix: ..."), then the verdict
/// lines. `colour`: ANSI colours on the state words (the text is complete without them).
std::string format_report(const DoctorReport& report, bool colour, bool pc_only);

/// One line per group: "Simulator: ready", "3DS: not ready (2 problems)", "Optional: 1 warning",
/// "Console: not checked (--console <ip> or --online)"; with `pc_only` the 3DS group's problems are "not
/// counted (--pc)".
std::vector<std::string> verdict_lines(const DoctorReport& report, bool pc_only);

/// 0 when no required group has a FAIL (the 3DS group doesn't count with `pc_only`), else 1. A warn never
/// counts.
int doctor_exit_code(const DoctorReport& report, bool pc_only);

} // namespace nnh::tool
