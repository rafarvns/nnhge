// nnh doctor's About and Simulator groups, and what the groups share (checks.hpp).

#include "doctor/checks.hpp"
#include "doctor/doctor_limits.hpp"
#include "doctor/tool_versions.hpp"
#include "repo.hpp"

#include <cctype>

namespace nnh::tool {

const char* const kSdl3Version = "3.4.20";
const char* const kSdl3MingwUrl =
    "https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-devel-3.4.20-mingw.tar.gz";
const char* const kSdl3SourceUrl =
    "https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz";

namespace {

DoctorCheck ok(const char* name, const std::string& found) {
    return {CheckState::Ok, name, found, ""};
}
DoctorCheck bad(CheckState s, const char* name, const std::string& found, const std::string& fix) {
    return {s, name, found, fix};
}

DoctorCheck cmake_check(const Probes& p, Host host) {
    const std::string path = p.find_program("cmake");
    if (path.empty())
        return bad(CheckState::Fail, "cmake", "not found on PATH", fix_for(Problem::InstallCmake, host));
    const Version v = cmake_version(p.run(path, {"--version"}).output);
    if (!v.valid())
        return bad(CheckState::Fail, "cmake", "version unknown (" + path + ")",
                   fix_for(Problem::OldCmake, host));
    const std::string found = v.text() + " (" + path + ")";
    if (v < limits::kCmake) {
        return bad(CheckState::Fail, "cmake", found + ": older than 3.25, the presets need 3.25",
                   fix_for(Problem::OldCmake, host));
    }
    return ok("cmake", found);
}

DoctorCheck ninja_check(const Probes& p, Host host) {
    const std::string path = p.find_program("ninja");
    if (path.empty())
        return bad(CheckState::Fail, "ninja", "not found on PATH", fix_for(Problem::InstallNinja, host));
    const Version v = ninja_version(p.run(path, {"--version"}).output);
    return ok("ninja", (v.valid() ? v.text() : std::string("version unknown")) + " (" + path + ")");
}

/// The compiler's verdict: FAIL below the oldest that builds the engine, warn below the checked ones, MSVC
/// and Apple clang best effort.
DoctorCheck judge_compiler(const CompilerVersion& c, const std::string& path, Host host) {
    const char* name = "C++ compiler";
    const char* kinds[] = {"", "GCC ", "clang ", "Apple clang ", "MSVC "};
    const std::string found = kinds[static_cast<int>(c.kind)] + c.version.text() + " (" + path + ")";
    switch (c.kind) {
    case CompilerKind::Gcc:
    case CompilerKind::Clang: {
        const bool gcc = c.kind == CompilerKind::Gcc;
        const Version min = gcc ? limits::kGccMin : limits::kClangMin,
                      checked = gcc ? limits::kGccChecked : limits::kClangChecked;
        if (c.version < min) {
            return bad(CheckState::Fail, name,
                       found + ": older than " + (gcc ? "GCC 12" : "clang 16") +
                           ", the oldest that builds the engine",
                       fix_for(Problem::OldCompiler, host));
        }
        if (c.version < checked) {
            return bad(CheckState::Warn, name,
                       found + ": not checked by the project (it checks " +
                           (gcc ? "GCC 13, 14 and 16" : "clang 18") + ")",
                       fix_for(Problem::OldCompiler, host));
        }
        return ok(name, found);
    }
    case CompilerKind::Msvc:
        if (c.version < limits::kMsvcMin) {
            return bad(CheckState::Fail, name, found + ": older than MSVC 19.30 (Visual Studio 2022)",
                       fix_for(Problem::MsvcBestEffort, host));
        }
        return bad(CheckState::Warn, name, found + ": best effort, not checked by the project",
                   fix_for(Problem::MsvcBestEffort, host));
    case CompilerKind::AppleClang:
        return bad(CheckState::Warn, name, found + ": best effort, not checked by the project",
                   fix_for(Problem::MacBestEffort, host));
    default:
        return bad(CheckState::Warn, name, "version unknown (" + path + ")",
                   fix_for(Problem::OldCompiler, host));
    }
}

DoctorCheck compiler_check(const Probes& p, const DoctorContext& c) {
    const std::string path = host_compiler_path(p, c);
    if (path.empty()) {
        const std::string cxx = p.env("CXX");
        return bad(CheckState::Fail, "C++ compiler",
                   cxx.empty() ? "no g++, c++, clang++ or cl on PATH"
                               : "CXX=" + cxx + " not found (a program name or a full path; a launcher such as "
                                                "ccache goes in CMAKE_CXX_COMPILER_LAUNCHER)",
                   fix_for(Problem::InstallCompiler, c.host));
    }
    const bool cl = path.size() >= 6 && path.compare(path.size() - 6, 6, "cl.exe") == 0;
    const RunResult r =
        cl ? p.run(path, {}) : p.run(path, {"--version"}); // cl prints its banner without arguments
    return judge_compiler(compiler_version(r.output), path, c.host);
}

bool pkg_config_has(const Probes& p, const std::string& pkg_config, const char* module) {
    const RunResult r = p.run(pkg_config, {"--exists", module});
    return r.started && r.exit_code == 0;
}

DoctorCheck sdl3_check(const Probes& p, Host host) {
    const std::string built =
        std::string("built from source at the first PC configure (SDL3 ") + kSdl3Version + ", pinned)";
    if (host == Host::Windows) {
        return ok("SDL3",
                  std::string("downloaded at the first PC configure (SDL3 ") + kSdl3Version + ", pinned)");
    }
    if (host == Host::MacOs) return ok("SDL3", built);
    const std::string pkg = p.find_program("pkg-config");
    if (pkg.empty()) {
        return bad(CheckState::Fail, "SDL3",
                   "no system SDL3 and no pkg-config: the source build can't find its dependencies",
                   fix_for(Problem::LinuxPkgConfig, host));
    }
    const RunResult system = p.run(pkg, {"--modversion", "sdl3"});
    if (system.started && system.exit_code == 0)
        return ok("SDL3", "system SDL3 " + first_version(system.output).text());
    if (!pkg_config_has(p, pkg, "x11") && !pkg_config_has(p, pkg, "wayland-client")) {
        return bad(CheckState::Fail, "SDL3",
                   "no X11 or Wayland development files: the simulator would have no window",
                   fix_for(Problem::LinuxWindowSystem, host));
    }
    if (!pkg_config_has(p, pkg, "alsa") && !pkg_config_has(p, pkg, "libpulse") &&
        !pkg_config_has(p, pkg, "libpipewire-0.3")) {
        return bad(CheckState::Warn, "SDL3",
                   built + "; no ALSA, PulseAudio or PipeWire development files: no sound",
                   fix_for(Problem::LinuxAudio, host));
    }
    return ok("SDL3", built);
}

} // namespace

std::string clean_folder(std::string path) {
    for (char& ch : path)
        ch = ch == '\\' ? '/' : ch;
    while (path.size() > 1 && path.back() == '/' && !(path.size() == 3 && path[1] == ':'))
        path.pop_back();
    return path;
}

std::string drive_form_hint(const std::string& value, Host host) {
    const bool msys = value.size() >= 2 && value[0] == '/' && std::isalpha(static_cast<unsigned char>(value[1])) &&
                      (value.size() == 2 || value[2] == '/');
    if (host != Host::Windows || !msys) return "";
    return "; write it in drive form: " + native_path(value);
}

std::string host_compiler_path(const Probes& p, const DoctorContext& c) {
    const std::string cxx = p.env("CXX");
    if (!cxx.empty()) {
        const std::string found = p.find_program(cxx.c_str());
        return found.empty() && p.is_file(cxx) ? cxx : found;
    }
    for (const char* name : {"c++", "g++", "clang++"}) {
        if (std::string path = p.find_program(name); !path.empty()) return path;
    }
    return c.host == Host::Windows ? p.find_program("cl") : "";
}

void check_about(const Probes& p, const DoctorContext& c, DoctorReport& r) {
    DoctorGroup g{"About", false, "", {ok("nnh", version_line(c.info))}};
    g.verdict = false;
    if (c.host == Host::MacOs) {
        g.checks.push_back(bad(CheckState::Warn, "system",
                               p.os_name() + ": best effort, not checked by the project",
                               fix_for(Problem::MacBestEffort, c.host)));
    } else {
        g.checks.push_back(ok("system", p.os_name()));
    }
    r.groups.push_back(g);
}

void check_simulator(const Probes& p, const DoctorContext& c, DoctorReport& r) {
    r.groups.push_back(
        {"Simulator",
         true,
         "",
         {cmake_check(p, c.host), ninja_check(p, c.host), compiler_check(p, c), sdl3_check(p, c.host)}});
}

DoctorReport run_doctor(const Probes& p, const DoctorContext& c) {
    DoctorReport r;
    check_about(p, c, r);
    check_simulator(p, c, r);
    check_3ds(p, c, r);
    check_optional(p, c, r);
    check_console(p, c, r);
    check_downloads(p, c, r);
    check_project(p, c, r);
    return r;
}

} // namespace nnh::tool
