#pragma once
// nnh doctor's checks (spec 033 FR-18, FR-19, FR-23, D56 item 5): one function per group, each pure over the
// probes (probes.hpp) and the context, each adding its group to the report. Windows-only checks are skipped
// on Linux and the reverse; macOS runs with "best effort" warnings. Tested with fake machines in
// tools/nnh/tests/test_doctor_*.cpp.

#include "build_info.hpp"
#include "doctor/doctor_report.hpp"
#include "doctor/fix_lines.hpp"
#include "doctor/probes.hpp"

#include <string>

namespace nnh::tool {

struct DoctorContext {
    BuildInfo info; ///< this nnh (the version line, the engine version it speaks)
    Host host = Host::Windows;
    bool pc_only = false;      ///< --pc
    bool online = false;       ///< --online
    std::string console;       ///< --console <ip> ("" = discovery with --online, nothing without it)
    std::string project_root;  ///< the project nnh would work in ("" = none here)
    bool game_project = false; ///< true: a game project (nnh.toml); false: the engine repository
};

void check_about(const Probes& p, const DoctorContext& c, DoctorReport& r);
void check_simulator(const Probes& p, const DoctorContext& c, DoctorReport& r);
void check_3ds(const Probes& p, const DoctorContext& c, DoctorReport& r);
void check_optional(const Probes& p, const DoctorContext& c, DoctorReport& r);
void check_console(const Probes& p, const DoctorContext& c, DoctorReport& r);
void check_downloads(const Probes& p, const DoctorContext& c, DoctorReport& r);
void check_project(const Probes& p, const DoctorContext& c, DoctorReport& r);

/// Every group, in the report's order.
DoctorReport run_doctor(const Probes& p, const DoctorContext& c);

// Shared by the groups (checks_host.cpp).
/// The host C++ compiler CMake would pick: CXX, else c++, g++, clang++ (and cl on Windows); "" when none.
std::string host_compiler_path(const Probes& p, const DoctorContext& c);
/// The devkitPro folder: DEVKITPRO (on Windows "/opt/devkitpro" is the MSYS2 mount of C:/devkitPro), else the
/// system's default (C:/devkitPro, /opt/devkitpro). `from_env` says whether DEVKITPRO named it.
std::string devkitpro_dir(const Probes& p, const DoctorContext& c, bool& from_env);
/// The SDL3 version and package URLs the engine pins (cmake/nnh_sdl3_pin.cmake; a test checks they are
/// equal).
extern const char* const kSdl3Version;
extern const char* const kSdl3MingwUrl;
extern const char* const kSdl3SourceUrl;
/// The fixed command nnh doctor runs in devkitPro's MSYS2 bash (`bash -lc`) to find CMake and Ninja there.
extern const char* const kDevkitProShellProbe;

} // namespace nnh::tool
