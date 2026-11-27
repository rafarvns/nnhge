#pragma once
// Helpers of nnh doctor's rule tests: a context per host, a machine with the usual Windows or Linux tools,
// and lookups in a report.

#include "doctor/checks.hpp"
#include "doctor_fakes.hpp"

#include <string>

namespace nnh::tool::test {

inline DoctorContext context(Host host) {
    DoctorContext c;
    c.info.version = "0.1.0";
    c.info.remote_protocol = 1;
    c.info.link_protocol = 1;
    c.info.os = host == Host::Windows ? "windows" : (host == Host::MacOs ? "macos" : "linux");
    c.info.arch = "x64";
    c.info.checkout_dir = "E:/src/nnhge";
    c.host = host;
    return c;
}

/// Windows with WinLibs GCC 16, CMake 4.4.2, Ninja 1.13.2 and devkitPro (MSYS2 with cmake and ninja, every
/// library).
inline FakeMachine windows_machine() {
    FakeMachine m;
    const std::string bin = "C:/w/mingw64/bin/";
    m.program("cmake", bin + "cmake.exe", "--version", "cmake version 4.4.2\n");
    m.program("ninja", bin + "ninja.exe", "--version", "1.13.2\n");
    m.program("c++", bin + "c++.exe", "--version",
              "c++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r1) 16.2.0\n");
    m.program("g++", bin + "g++.exe", "--version",
              "g++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r1) 16.2.0\n");
    m.files.insert("C:/devkitPro/cmake/3DS.cmake");
    m.files.insert("C:/devkitPro/devkitARM/bin/arm-none-eabi-gcc.exe");
    m.files.insert("C:/devkitPro/libctru/lib/libctru.a");
    m.files.insert("C:/devkitPro/libctru/lib/libcitro2d.a");
    m.files.insert("C:/devkitPro/libctru/lib/libcitro3d.a");
    m.files.insert("C:/devkitPro/msys2/usr/bin/bash.exe");
    m.dirs["C:/devkitPro/msys2/var/lib/pacman/local"] = {"citro2d-1.7.0-1", "citro3d-1.7.1-2",
                                                         "cmake-4.0.2-1",   "devkitARM-r68-1",
                                                         "libctru-2.7.1-1", "ninja-1.13.2-2"};
    m.outputs["C:/devkitPro/msys2/usr/bin/bash.exe -lc " + std::string(kDevkitProShellProbe)] =
        RunResult{true, 0, false,
                  "nnh-cmake=/usr/bin/cmake\nnnh-ninja=/usr/bin/ninja\ncmake version "
                  "4.0.2\nnnh-ninja-version=1.13.2\n"};
    return m;
}

/// Ubuntu with GCC 13, CMake 3.28, Ninja, pkg-config, X11/Wayland and audio development files, devkitPro in
/// /opt/devkitpro with DEVKITPRO set.
inline FakeMachine linux_machine() {
    FakeMachine m;
    m.os = "Ubuntu 24.04.1 LTS";
    m.program("cmake", "/usr/bin/cmake", "--version", "cmake version 3.28.3\n");
    m.program("ninja", "/usr/bin/ninja", "--version", "1.11.1\n");
    m.program("c++", "/usr/bin/c++", "--version", "c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0\n");
    m.program("g++", "/usr/bin/g++", "--version", "g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0\n");
    m.program("pkg-config", "/usr/bin/pkg-config", "--exists x11", "");
    m.outputs["/usr/bin/pkg-config --exists wayland-client"] = RunResult{true, 0, false, ""};
    m.outputs["/usr/bin/pkg-config --exists alsa"] = RunResult{true, 0, false, ""};
    m.outputs["/usr/bin/pkg-config --modversion sdl3"] =
        RunResult{true, 1, false, "Package sdl3 was not found\n"};
    m.env["DEVKITPRO"] = "/opt/devkitpro";
    m.files.insert("/opt/devkitpro/cmake/3DS.cmake");
    m.files.insert("/opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc");
    m.files.insert("/opt/devkitpro/libctru/lib/libctru.a");
    m.files.insert("/opt/devkitpro/libctru/lib/libcitro2d.a");
    m.files.insert("/opt/devkitpro/libctru/lib/libcitro3d.a");
    m.dirs["/opt/devkitpro/pacman/var/lib/pacman/local"] = {"citro2d-1.7.0-1", "citro3d-1.7.1-2",
                                                            "devkitARM-r68-1", "libctru-2.7.0-1"};
    return m;
}

inline const DoctorGroup* group(const DoctorReport& r, const char* name) {
    for (const DoctorGroup& g : r.groups) {
        if (g.name == name) return &g;
    }
    return nullptr;
}

/// The check `name` of group `group_name` (an empty check with state Ok and name "<missing>" when there is
/// none).
inline DoctorCheck check(const DoctorReport& r, const char* group_name, const char* name) {
    if (const DoctorGroup* g = group(r, group_name)) {
        for (const DoctorCheck& c : g->checks) {
            if (c.name == name) return c;
        }
    }
    return DoctorCheck{CheckState::Ok, "<missing>", "", ""};
}

inline bool has(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

} // namespace nnh::tool::test
