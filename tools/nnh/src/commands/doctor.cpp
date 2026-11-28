// nnh doctor (spec 033 FR-18 to FR-21, D56 item 5): reads this machine and the project here through the real
// probes, prints the report and returns 0 (ready) or 1. Writes nothing; the network library starts only for
// --console/--online.

#include "build_info.hpp"
#include "commands.hpp"
#include "doctor/checks.hpp"
#include "link_types.hpp"
#include "project_root.hpp"
#include "terminal.hpp"
#include "text_file.hpp"

#include <cstdio>
#include <filesystem>

namespace nnh::tool {
namespace {

Host this_host() {
#if defined(_WIN32)
    return Host::Windows;
#elif defined(__APPLE__)
    return Host::MacOs;
#else
    return linux_host(read_text_file("/etc/os-release", 16 * 1024));
#endif
}

std::string current_folder() {
    std::error_code ec;
    const std::u8string u8 = std::filesystem::current_path(ec).generic_u8string();
    return std::string(u8.begin(), u8.end());
}

} // namespace

int command_doctor(const Options& options) {
    DoctorContext c;
    c.info = build_info();
    c.host = this_host();
    c.pc_only = options.pc;
    c.online = options.online;
    c.console = options.console;
    const ProjectRoot root = find_project_root(current_folder());
    c.project_root = root.path;
    c.game_project = root.game;
    if ((c.online || !c.console.empty()) && !net::startup()) {
        std::fprintf(stderr, "nnh: the network library didn't start\n");
        return 1;
    }
    const DoctorReport report = run_doctor(system_probes(), c);
    std::fputs(format_report(report, enable_colour(), c.pc_only).c_str(), stdout);
    return doctor_exit_code(report, c.pc_only);
}

} // namespace nnh::tool
