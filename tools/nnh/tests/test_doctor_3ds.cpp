// nnh doctor's 3DS and Optional groups on fake machines (doctor/checks_3ds.cpp, checks_optional.cpp; spec 033
// FR-19, FR-23, AC-6).

#include "doctor/checks.hpp"
#include "doctor_checks_util.hpp"
#include "test.hpp"

using namespace nnh::tool;
using namespace nnh::tool::test;

namespace {

DoctorReport tds(const FakeMachine& m, Host host) {
    DoctorReport r;
    check_3ds(m.probes(), context(host), r);
    return r;
}

} // namespace

NNH_TEST(a_ready_windows_devkitpro_is_all_ok) {
    const DoctorReport r = tds(windows_machine(), Host::Windows);
    const DoctorCheck dkp = check(r, "3DS", "devkitPro");
    CHECK(dkp.state == CheckState::Ok && has(dkp.found, "C:/devkitPro") && has(dkp.found, "DEVKITPRO unset"));
    CHECK(check(r, "3DS", "devkitARM").found == "r68 (C:/devkitPro/devkitARM)");
    CHECK(check(r, "3DS", "libctru").found == "2.7.1");
    CHECK(check(r, "3DS", "citro2d").found == "1.7.0" && check(r, "3DS", "citro3d").found == "1.7.1");
    const DoctorCheck shell = check(r, "3DS", "devkitPro shell");
    CHECK(shell.state == CheckState::Ok && shell.found == "C:/devkitPro/msys2 (cmake 4.0.2, ninja 1.13.2)");
    CHECK(check(r, "3DS", "host compiler").found == "C:/w/mingw64/bin/g++.exe");
    CHECK(doctor_exit_code(r, false) == 0);
}

NNH_TEST(windows_devkitpro_from_the_environment_or_its_msys_form) {
    FakeMachine m = windows_machine();
    m.env["DEVKITPRO"] = "/opt/devkitpro"; // what devkitPro's installer sets: the MSYS2 mount of C:/devkitPro
    DoctorCheck dkp = check(tds(m, Host::Windows), "3DS", "devkitPro");
    CHECK(dkp.state == CheckState::Ok && dkp.found == "C:/devkitPro");
    m.env["DEVKITPRO"] = "D:/dkp";
    dkp = check(tds(m, Host::Windows), "3DS", "devkitPro");
    CHECK(dkp.state == CheckState::Fail && has(dkp.found, "D:/dkp") &&
          dkp.fix == "https://github.com/devkitPro/installer/releases");
}

NNH_TEST(devkitpro_missing_fails_with_the_installer_link) {
    FakeMachine m;
    m.program("g++", "C:/w/g++.exe", "--version", "g++ (GCC) 16.2.0\n");
    const DoctorReport r = tds(m, Host::Windows);
    const DoctorCheck dkp = check(r, "3DS", "devkitPro");
    CHECK(dkp.state == CheckState::Fail && dkp.fix == "https://github.com/devkitPro/installer/releases");
    CHECK(check(r, "3DS", "libctru").name == "<missing>"); // nothing more to look at without devkitPro
    CHECK(doctor_exit_code(r, false) == 1 && doctor_exit_code(r, true) == 0);
}

NNH_TEST(linux_needs_devkitpro_set_and_says_how) {
    FakeMachine m = linux_machine();
    CHECK(check(tds(m, Host::Debian), "3DS", "devkitPro").found == "/opt/devkitpro");
    m.env.erase("DEVKITPRO");
    DoctorCheck dkp = check(tds(m, Host::Debian), "3DS", "devkitPro");
    CHECK(dkp.state == CheckState::Fail && has(dkp.found, "DEVKITPRO is not set") &&
          dkp.fix == "export DEVKITPRO=/opt/devkitpro");
    m.files.erase("/opt/devkitpro/cmake/3DS.cmake");
    dkp = check(tds(m, Host::Debian), "3DS", "devkitPro");
    CHECK(dkp.state == CheckState::Fail && dkp.fix == "https://devkitpro.org/wiki/devkitPro_pacman");
}

NNH_TEST(missing_libraries_fail_with_the_pacman_line) {
    FakeMachine m = windows_machine();
    m.files.erase("C:/devkitPro/libctru/lib/libcitro3d.a");
    DoctorCheck c = check(tds(m, Host::Windows), "3DS", "citro3d");
    CHECK(c.state == CheckState::Fail &&
          c.fix == "C:/devkitPro/msys2/usr/bin/bash.exe -lc \"pacman -S --needed 3ds-dev\"");
    FakeMachine l = linux_machine();
    l.files.erase("/opt/devkitpro/libctru/lib/libctru.a");
    c = check(tds(l, Host::Debian), "3DS", "libctru");
    CHECK(c.state == CheckState::Fail && c.fix == "sudo dkp-pacman -S --needed 3ds-dev");
    l = linux_machine();
    l.dirs.clear(); // no pacman database: still ok, version unknown
    CHECK(check(tds(l, Host::Debian), "3DS", "libctru").state == CheckState::Ok);
    CHECK(check(tds(l, Host::Debian), "3DS", "libctru").found == "version unknown");
}

NNH_TEST(the_devkitpro_shell_needs_cmake_and_ninja_inside) {
    FakeMachine m = windows_machine();
    const std::string key = "C:/devkitPro/msys2/usr/bin/bash.exe -lc " + std::string(kDevkitProShellProbe);
    m.outputs[key].output = "nnh-cmake=/usr/bin/cmake\nnnh-ninja=\ncmake version 4.0.2\nnnh-ninja-version=\n";
    DoctorCheck s = check(tds(m, Host::Windows), "3DS", "devkitPro shell");
    CHECK(s.state == CheckState::Fail && has(s.found, "no ninja"));
    CHECK(s.fix == "C:/devkitPro/msys2/usr/bin/bash.exe -lc \"pacman -S --needed cmake ninja\"");
    m.outputs[key].output = "nnh-cmake=/usr/bin/cmake\nnnh-ninja=/usr/bin/ninja\ncmake version "
                            "3.20.0\nnnh-ninja-version=1.13.2\n";
    s = check(tds(m, Host::Windows), "3DS", "devkitPro shell");
    CHECK(s.state == CheckState::Fail && has(s.found, "cmake 3.20.0"));
    m.env["DEVKITPRO_MSYS2"] = "D:/other/msys2"; // DEVKITPRO_MSYS2 points elsewhere: that one is checked
    s = check(tds(m, Host::Windows), "3DS", "devkitPro shell");
    CHECK(s.state == CheckState::Fail && has(s.found, "D:/other/msys2") && has(s.fix, "devkitPro/installer"));
    CHECK(check(tds(linux_machine(), Host::Debian), "3DS", "devkitPro shell").name ==
          "<missing>"); // Windows only
}

NNH_TEST(no_host_compiler_for_3ds_configures_fails) {
    FakeMachine m = windows_machine();
    m.programs.erase("g++");
    m.programs.erase("c++");
    DoctorCheck h = check(tds(m, Host::Windows), "3DS", "host compiler");
    CHECK(h.state == CheckState::Fail && h.fix == "winget install BrechtSanders.WinLibs.POSIX.UCRT");
    m.env["NNH_HOST_CXX"] = "C:/other/g++.exe";
    h = check(tds(m, Host::Windows), "3DS", "host compiler");
    CHECK(h.state == CheckState::Ok && h.found == "C:/other/g++.exe (NNH_HOST_CXX)");
}

NNH_TEST(optional_tools_never_fail) {
    FakeMachine m = windows_machine();
    DoctorReport r;
    check_optional(m.probes(), context(Host::Windows), r);
    const DoctorGroup* g = group(r, "Optional");
    CHECK(g && !g->required && g->checks.size() == 3);
    for (const DoctorCheck& c : g->checks)
        CHECK(c.state == CheckState::Warn && !c.fix.empty());
    CHECK(check(r, "Optional", "ccache").fix == "winget install Ccache.Ccache");
    CHECK(check(r, "Optional", "git").fix == "winget install Git.Git");
    CHECK(check(r, "Optional", "Azahar").fix == "https://azahar-emu.org/");
    m.program("ccache", "C:/w/ccache.exe", "--version",
              "ccache version 4.10.2\nFeatures: file-storage http-storage\n");
    m.program("git", "C:/git/cmd/git.exe", "--version", "git version 2.47.1.windows.1\n");
    DoctorReport ok;
    check_optional(m.probes(), context(Host::Windows), ok);
    CHECK(check(ok, "Optional", "ccache").found == "4.10.2 (C:/w/ccache.exe)");
    CHECK(check(ok, "Optional", "git").state == CheckState::Ok);
    CHECK(doctor_exit_code(ok, false) == 0);
}

NNH_TEST(the_msys2_folder_is_normalised_and_a_space_gets_the_docs_link) { // review 033
    FakeMachine m = windows_machine();
    m.env["DEVKITPRO_MSYS2"] = "C:\\devkitPro\\msys2\\";
    DoctorCheck s = check(tds(m, Host::Windows), "3DS", "devkitPro shell");
    CHECK(s.state == CheckState::Ok && s.found == "C:/devkitPro/msys2 (cmake 4.0.2, ninja 1.13.2)");
    m.files.erase("C:/devkitPro/libctru/lib/libctru.a");
    CHECK(check(tds(m, Host::Windows), "3DS", "libctru").fix ==
          "C:/devkitPro/msys2/usr/bin/bash.exe -lc \"pacman -S --needed 3ds-dev\"");
    m.env["DEVKITPRO_MSYS2"] = "D:/my tools/msys2";
    m.files.insert("D:/my tools/msys2/usr/bin/bash.exe");
    m.outputs["D:/my tools/msys2/usr/bin/bash.exe -lc " + std::string(kDevkitProShellProbe)] =
        RunResult{true, 0, false, "nnh-cmake=/usr/bin/cmake\nnnh-ninja=\ncmake version 4.0.2\nnnh-ninja-version=\n"};
    s = check(tds(m, Host::Windows), "3DS", "devkitPro shell");
    CHECK(s.state == CheckState::Fail && s.fix == "https://devkitpro.org/wiki/devkitPro_pacman"); // no quoted program
    CHECK(check(tds(m, Host::Windows), "3DS", "libctru").fix == "https://devkitpro.org/wiki/devkitPro_pacman");
}

NNH_TEST(an_msys_form_path_outside_msys_gets_the_drive_form_hint) {
    FakeMachine m = windows_machine();
    m.env["DEVKITPRO"] = "/d/devkitPro";
    const DoctorCheck d = check(tds(m, Host::Windows), "3DS", "devkitPro");
    CHECK(d.state == CheckState::Fail && has(d.found, "write it in drive form: D:/devkitPro"));
}
