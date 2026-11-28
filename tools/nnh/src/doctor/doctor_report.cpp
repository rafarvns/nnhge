// nnh doctor's report (doctor_report.hpp).

#include "doctor/doctor_report.hpp"

namespace nnh::tool {
namespace {

constexpr std::size_t kNameWidth = 16; // "NNHGE_ENGINE_DIR"

const char* word(CheckState s) {
    return s == CheckState::Ok ? "ok" : (s == CheckState::Warn ? "warn" : "FAIL");
}
const char* colour_of(CheckState s) {
    return s == CheckState::Ok ? "\x1b[32m" : (s == CheckState::Warn ? "\x1b[33m" : "\x1b[31m");
}

int count(const DoctorGroup& g, CheckState state) {
    int n = 0;
    for (const DoctorCheck& c : g.checks)
        n += c.state == state ? 1 : 0;
    return n;
}

std::string plural(int n, const char* one, const char* many) {
    return std::to_string(n) + " " + (n == 1 ? one : many);
}

bool is_3ds(const DoctorGroup& g) {
    return g.name == "3DS";
}

std::string verdict(const DoctorGroup& g, bool pc_only) {
    if (!g.not_checked.empty()) return g.name + ": not checked (" + g.not_checked + ")";
    const int problems = count(g, CheckState::Fail), warnings = count(g, CheckState::Warn);
    if (!g.required) {
        if (problems > 0) return g.name + ": " + plural(problems, "problem", "problems") + " (not required)";
        return g.name + ": " + (warnings > 0 ? plural(warnings, "warning", "warnings") : std::string("ok"));
    }
    if (problems > 0) {
        return g.name + ": not ready (" + plural(problems, "problem", "problems") +
               (pc_only && is_3ds(g) ? "; not counted: --pc)" : ")");
    }
    return g.name + ": ready" + (warnings > 0 ? " (" + plural(warnings, "warning", "warnings") + ")" : "");
}

} // namespace

std::string format_report(const DoctorReport& report, bool colour, bool pc_only) {
    std::string out;
    for (const DoctorGroup& g : report.groups) {
        if (!g.not_checked.empty()) continue;
        out += g.name + "\n";
        for (const DoctorCheck& c : g.checks) {
            const std::string state = word(c.state);
            out += "  ";
            out += colour ? colour_of(c.state) + state + "\x1b[0m" : state;
            out += std::string(6 - state.size(), ' ');
            out += c.name + std::string(c.name.size() < kNameWidth ? kNameWidth + 1 - c.name.size() : 1, ' ');
            out += c.found + "\n";
            if (c.state != CheckState::Ok && !c.fix.empty()) out += "        fix: " + c.fix + "\n";
        }
    }
    out += "\n";
    for (const std::string& line : verdict_lines(report, pc_only))
        out += line + "\n";
    return out;
}

std::vector<std::string> verdict_lines(const DoctorReport& report, bool pc_only) {
    std::vector<std::string> lines;
    for (const DoctorGroup& g : report.groups) {
        if (g.verdict) lines.push_back(verdict(g, pc_only));
    }
    return lines;
}

int doctor_exit_code(const DoctorReport& report, bool pc_only) {
    for (const DoctorGroup& g : report.groups) {
        if (!g.required || !g.not_checked.empty() || (pc_only && is_3ds(g))) continue;
        if (count(g, CheckState::Fail) > 0) return 1;
    }
    return 0;
}

} // namespace nnh::tool
