// nnh doctor's About and Simulator groups on fake machines (doctor/checks_host.cpp, spec 033 FR-19, FR-23,
// AC-6).

#include "doctor/checks.hpp"
#include "doctor_checks_util.hpp"
#include "test.hpp"

using namespace nnh::tool;
using namespace nnh::tool::test;

namespace {

DoctorReport simulator(const FakeMachine& m, Host host) {
    DoctorReport r;
    check_simulator(m.probes(), context(host), r);
    return r;
}

} // namespace

NNH_TEST(about_names_this_nnh_and_the_system) {
    DoctorReport r;
    check_about(windows_machine().probes(), context(Host::Windows), r);
    const DoctorCheck nnh = check(r, "About", "nnh");
    CHECK(nnh.state == CheckState::Ok &&
          nnh.found == "nnh 0.1.0 (protocol 1, link 1, windows-x64, from a source checkout)");
    const DoctorCheck system = check(r, "About", "system");
    CHECK(system.state == CheckState::Ok && system.found == "Windows 11 (10.0.26100)");
    CHECK(group(r, "About") && !group(r, "About")->required && !group(r, "About")->verdict);
}

NNH_TEST(macos_is_best_effort) {
    FakeMachine m;
    m.os = "macOS";
    DoctorReport r;
    check_about(m.probes(), context(Host::MacOs), r);
    const DoctorCheck system = check(r, "About", "system");
    CHECK(system.state == CheckState::Warn && has(system.found, "best effort, not checked by the project"));
    CHECK(has(system.fix, "getting-started.md"));
}

NNH_TEST(a_ready_windows_simulator_is_all_ok) {
    const DoctorReport r = simulator(windows_machine(), Host::Windows);
    CHECK(group(r, "Simulator") && group(r, "Simulator")->required);
    CHECK(check(r, "Simulator", "cmake").state == CheckState::Ok);
    CHECK(check(r, "Simulator", "cmake").found == "4.4.2 (C:/w/mingw64/bin/cmake.exe)");
    CHECK(check(r, "Simulator", "ninja").found == "1.13.2 (C:/w/mingw64/bin/ninja.exe)");
    const DoctorCheck cxx = check(r, "Simulator", "C++ compiler");
    CHECK(cxx.state == CheckState::Ok && cxx.found == "GCC 16.2.0 (C:/w/mingw64/bin/c++.exe)");
    const DoctorCheck sdl = check(r, "Simulator", "SDL3");
    CHECK(sdl.state == CheckState::Ok && has(sdl.found, "3.4.20") && has(sdl.found, "first PC configure"));
    CHECK(doctor_exit_code(r, false) == 0);
}

NNH_TEST(cmake_missing_or_older_than_3_25_fails) {
    FakeMachine m = windows_machine();
    m.outputs["C:/w/mingw64/bin/cmake.exe --version"].output = "cmake version 3.20.0\n";
    DoctorCheck c = check(simulator(m, Host::Windows), "Simulator", "cmake");
    CHECK(c.state == CheckState::Fail && has(c.found, "3.20.0") && has(c.found, "3.25") &&
          c.fix == "https://cmake.org/download/");
    m.outputs["C:/w/mingw64/bin/cmake.exe --version"].output = "cmake version 3.25.0\n";
    CHECK(check(simulator(m, Host::Windows), "Simulator", "cmake").state == CheckState::Ok);
    m.programs.erase("cmake");
    c = check(simulator(m, Host::Windows), "Simulator", "cmake");
    CHECK(c.state == CheckState::Fail && c.found == "not found on PATH" &&
          c.fix == "winget install Kitware.CMake");
    FakeMachine l = linux_machine();
    l.programs.erase("cmake");
    CHECK(check(simulator(l, Host::Debian), "Simulator", "cmake").fix == "sudo apt install cmake");
}

NNH_TEST(ninja_missing_fails_with_the_install_line) {
    FakeMachine m = windows_machine();
    m.programs.erase("ninja");
    const DoctorReport r = simulator(m, Host::Windows);
    const DoctorCheck n = check(r, "Simulator", "ninja");
    CHECK(n.state == CheckState::Fail && n.fix == "winget install Ninja-build.Ninja");
    CHECK(doctor_exit_code(r, false) == 1);
    FakeMachine l = linux_machine();
    l.programs.erase("ninja");
    CHECK(check(simulator(l, Host::Debian), "Simulator", "ninja").fix == "sudo apt install ninja-build");
}

NNH_TEST(compilers_below_the_minimum_fail_and_unchecked_ones_warn) {
    FakeMachine m = linux_machine();
    m.outputs["/usr/bin/c++ --version"].output = "c++ (Ubuntu 12.4.0-2ubuntu1~24.04.1) 12.4.0\n";
    DoctorCheck c = check(simulator(m, Host::Debian), "Simulator", "C++ compiler");
    CHECK(c.state == CheckState::Warn && has(c.found, "GCC 12.4.0") &&
          has(c.found, "not checked by the project"));
    CHECK(c.fix == "sudo apt install g++-13");
    m.outputs["/usr/bin/c++ --version"].output = "c++ (GCC) 11.4.0\n";
    c = check(simulator(m, Host::Debian), "Simulator", "C++ compiler");
    CHECK(c.state == CheckState::Fail && has(c.found, "GCC 11.4.0") && has(c.found, "GCC 12"));
    m.outputs["/usr/bin/c++ --version"].output = "Ubuntu clang version 17.0.6 (9ubuntu1)\n";
    CHECK(check(simulator(m, Host::Debian), "Simulator", "C++ compiler").state == CheckState::Warn);
    m.outputs["/usr/bin/c++ --version"].output = "Ubuntu clang version 15.0.7\n";
    CHECK(check(simulator(m, Host::Debian), "Simulator", "C++ compiler").state == CheckState::Fail);
    m.outputs["/usr/bin/c++ --version"].output = "Ubuntu clang version 18.1.3 (1ubuntu1)\n";
    CHECK(check(simulator(m, Host::Debian), "Simulator", "C++ compiler").state == CheckState::Ok);
}

NNH_TEST(the_compiler_follows_cxx_then_the_usual_names) {
    FakeMachine m = linux_machine();
    m.env["CXX"] = "clang++-18";
    m.program("clang++-18", "/usr/bin/clang++-18", "--version", "Ubuntu clang version 18.1.3 (1ubuntu1)\n");
    const DoctorCheck c = check(simulator(m, Host::Debian), "Simulator", "C++ compiler");
    CHECK(c.state == CheckState::Ok && c.found == "clang 18.1.3 (/usr/bin/clang++-18)");
    FakeMachine none = linux_machine();
    none.programs.erase("c++");
    none.programs.erase("g++");
    const DoctorCheck missing = check(simulator(none, Host::Debian), "Simulator", "C++ compiler");
    CHECK(missing.state == CheckState::Fail && missing.fix == "sudo apt install g++");
}

NNH_TEST(msvc_and_apple_clang_are_best_effort) {
    FakeMachine m = windows_machine();
    m.programs.erase("c++");
    m.programs.erase("g++");
    m.program("cl", "C:/vs/cl.exe", "",
              "Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35211 for x64\r\n");
    DoctorCheck c = check(simulator(m, Host::Windows), "Simulator", "C++ compiler");
    CHECK(c.state == CheckState::Warn && has(c.found, "MSVC 19.44") && has(c.found, "best effort"));
    m.outputs["C:/vs/cl.exe"].output =
        "Microsoft (R) C/C++ Optimizing Compiler Version 19.29.30154 for x64\r\n";
    CHECK(check(simulator(m, Host::Windows), "Simulator", "C++ compiler").state == CheckState::Fail);
    FakeMachine mac = linux_machine();
    mac.outputs["/usr/bin/c++ --version"].output = "Apple clang version 15.0.0 (clang-1500.3.9.4)\n";
    c = check(simulator(mac, Host::MacOs), "Simulator", "C++ compiler");
    CHECK(c.state == CheckState::Warn && has(c.found, "Apple clang 15.0.0") && has(c.found, "best effort"));
}

NNH_TEST(linux_sdl3_needs_pkg_config_and_a_window_system) {
    FakeMachine m = linux_machine();
    DoctorCheck s = check(simulator(m, Host::Debian), "Simulator", "SDL3");
    CHECK(s.state == CheckState::Ok && has(s.found, "built from source at the first PC configure"));
    m.outputs["/usr/bin/pkg-config --modversion sdl3"] = RunResult{true, 0, false, "3.2.10\n"};
    CHECK(check(simulator(m, Host::Debian), "Simulator", "SDL3").found == "system SDL3 3.2.10");
    m = linux_machine();
    m.outputs.erase("/usr/bin/pkg-config --exists alsa");
    s = check(simulator(m, Host::Debian), "Simulator", "SDL3");
    CHECK(s.state == CheckState::Warn && has(s.found, "no sound") &&
          s.fix == "sudo apt install libasound2-dev libpulse-dev");
    m.outputs.erase("/usr/bin/pkg-config --exists x11");
    m.outputs.erase("/usr/bin/pkg-config --exists wayland-client");
    s = check(simulator(m, Host::Debian), "Simulator", "SDL3");
    CHECK(s.state == CheckState::Fail && has(s.found, "no window") && has(s.fix, "libx11-dev"));
    m.programs.erase("pkg-config");
    s = check(simulator(m, Host::OtherLinux), "Simulator", "SDL3");
    CHECK(s.state == CheckState::Fail && has(s.found, "pkg-config") && has(s.fix, "wiki.libsdl.org"));
}

NNH_TEST(debian_family_comes_from_os_release) {
    CHECK(linux_host("NAME=\"Ubuntu\"\nID=ubuntu\nID_LIKE=debian\n") == Host::Debian);
    CHECK(linux_host("ID=linuxmint\nID_LIKE=\"ubuntu debian\"\n") == Host::Debian);
    CHECK(linux_host("ID=debian\n") == Host::Debian);
    CHECK(linux_host("ID=fedora\n") == Host::OtherLinux);
    CHECK(linux_host("ID=arch\nID_LIKE=archlinux\n") == Host::OtherLinux);
    CHECK(linux_host("") == Host::OtherLinux);
}

NNH_TEST(a_cxx_that_is_not_a_program_is_named) { // review 033: CXX="ccache g++"
    FakeMachine m = linux_machine();
    m.env["CXX"] = "ccache g++";
    const DoctorCheck c = check(simulator(m, Host::Debian), "Simulator", "C++ compiler");
    CHECK(c.state == CheckState::Fail && has(c.found, "CXX=ccache g++ not found") && has(c.found, "LAUNCHER"));
}
