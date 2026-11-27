// nnh doctor's 3DS group (checks.hpp): devkitPro, its libraries, its MSYS2 shell on Windows, the host
// compiler 3DS configures need.

#include "doctor/checks.hpp"
#include "doctor/doctor_limits.hpp"
#include "doctor/tool_versions.hpp"
#include "host_compiler.hpp"

namespace nnh::tool {

const char* const kDevkitProShellProbe =
    "echo nnh-cmake=$(command -v cmake); echo nnh-ninja=$(command -v ninja); "
    "cmake --version 2>/dev/null | head -n 1; echo nnh-ninja-version=$(ninja --version 2>/dev/null)";

namespace {

DoctorCheck ok(const char* name, const std::string& found) {
    return {CheckState::Ok, name, found, ""};
}
DoctorCheck fail(const char* name, const std::string& found, const std::string& fix) {
    return {CheckState::Fail, name, found, fix};
}

/// The value after `marker` up to the end of its line ("" when absent).
std::string marked(const std::string& text, const std::string& marker) {
    const std::size_t at = text.find(marker);
    if (at == std::string::npos) return "";
    const std::size_t start = at + marker.size();
    return text.substr(start, text.find_first_of("\r\n", start) - start);
}

std::string msys2_dir(const Probes& p, const std::string& devkitpro) {
    const std::string env = p.env("DEVKITPRO_MSYS2");
    return env.empty() ? devkitpro + "/msys2" : env;
}

DoctorCheck shell_check(const Probes& p, Host host, const std::string& msys2) {
    const std::string bash = msys2 + "/usr/bin/bash.exe";
    if (!p.is_file(bash))
        return fail("devkitPro shell", "no " + bash, fix_for(Problem::InstallDevkitPro, host));
    const RunResult r = p.run(bash, {"-lc", kDevkitProShellProbe});
    const std::string fix = fix_for(Problem::DevkitProShellTools, host, msys2);
    if (!r.started || r.timed_out) return fail("devkitPro shell", bash + " did not answer in 10 s", fix);
    const bool cmake = !marked(r.output, "nnh-cmake=").empty(),
               ninja = !marked(r.output, "nnh-ninja=").empty();
    const Version cv =
        cmake_version(r.output.substr(std::min(r.output.find("cmake version"), r.output.size())));
    const std::string ninja_text = marked(r.output, "nnh-ninja-version=");
    if (!cmake || !ninja) {
        return fail("devkitPro shell",
                    msys2 + ": " +
                        (!cmake && !ninja ? "no cmake, no ninja" : (!cmake ? "no cmake" : "no ninja")) +
                        " inside (nnh builds for the 3DS there)",
                    fix);
    }
    if (!cv.valid() || cv < limits::kDevkitProCmake) {
        return fail("devkitPro shell",
                    msys2 + ": cmake " + (cv.valid() ? cv.text() : std::string("of unknown version")) +
                        " inside, older than 3.25",
                    fix);
    }
    return ok("devkitPro shell", msys2 + " (cmake " + cv.text() + ", ninja " + ninja_text + ")");
}

DoctorCheck host_cxx_check(const Probes& p, Host host) {
    const std::string env = p.env("NNH_HOST_CXX");
    const std::string cxx =
        find_host_cxx(env.empty() ? nullptr : env.c_str(), [&](const char* n) { return p.find_program(n); });
    if (cxx.empty()) {
        return fail("host compiler",
                    "no g++, c++ or clang++ for the 3DS build's host programs (or set NNH_HOST_CXX)",
                    fix_for(Problem::HostCompilerFor3ds, host));
    }
    return ok("host compiler", env.empty() ? cxx : cxx + " (NNH_HOST_CXX)");
}

} // namespace

std::string devkitpro_dir(const Probes& p, const DoctorContext& c, bool& from_env) {
    std::string dir = p.env("DEVKITPRO");
    from_env = !dir.empty();
    if (c.host == Host::Windows && (dir.empty() || dir == "/opt/devkitpro" || dir == "/opt/devkitpro/"))
        return "C:/devkitPro";
    if (dir.empty()) return "/opt/devkitpro";
    while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\'))
        dir.pop_back();
    for (char& ch : dir)
        ch = ch == '\\' ? '/' : ch;
    return dir;
}

void check_3ds(const Probes& p, const DoctorContext& c, DoctorReport& r) {
    DoctorGroup g{"3DS", true, "", {}};
    bool from_env = false;
    const std::string dkp = devkitpro_dir(p, c, from_env);
    const bool windows = c.host == Host::Windows;
    if (!p.is_file(dkp + "/cmake/3DS.cmake")) {
        g.checks.push_back(fail("devkitPro",
                                (from_env ? "DEVKITPRO=" + p.env("DEVKITPRO") + ": " : std::string()) +
                                    "no " + dkp + "/cmake/3DS.cmake (devkitPro's 3DS toolchain)",
                                fix_for(Problem::InstallDevkitPro, c.host)));
        if (windows) g.checks.push_back(host_cxx_check(p, c.host));
        r.groups.push_back(g);
        return;
    }
    if (!windows && !from_env) {
        g.checks.push_back(fail("devkitPro",
                                dkp + " found, but DEVKITPRO is not set (the 3DS presets read it)",
                                fix_for(Problem::SetDevkitPro, c.host)));
    } else {
        g.checks.push_back(
            ok("devkitPro",
               windows && !from_env ? dkp + " (DEVKITPRO unset: fine, nnh's devkitPro shell sets it)" : dkp));
    }
    const std::string msys2 = msys2_dir(p, dkp);
    const std::vector<std::string> packages =
        p.list_dir(windows ? msys2 + "/var/lib/pacman/local" : dkp + "/pacman/var/lib/pacman/local");
    const auto version_of = [&](const char* package) {
        const std::string v = pacman_version(packages, package);
        return v.empty() ? std::string("version unknown") : v;
    };
    const std::string pacman_fix = fix_for(Problem::Install3dsLibraries, c.host, msys2);
    const std::string exe = windows ? ".exe" : "";
    if (p.is_file(dkp + "/devkitARM/bin/arm-none-eabi-gcc" + exe)) {
        g.checks.push_back(ok("devkitARM", version_of("devkitARM") + " (" + dkp + "/devkitARM)"));
    } else {
        g.checks.push_back(fail("devkitARM", "no " + dkp + "/devkitARM", pacman_fix));
    }
    const char* const libraries[][2] = {
        {"libctru", "libctru.a"}, {"citro2d", "libcitro2d.a"}, {"citro3d", "libcitro3d.a"}};
    for (const auto& [name, file] : libraries) {
        if (p.is_file(dkp + "/libctru/lib/" + file))
            g.checks.push_back(ok(name, version_of(name)));
        else
            g.checks.push_back(fail(name, "no " + dkp + "/libctru/lib/" + file, pacman_fix));
    }
    if (windows) g.checks.push_back(shell_check(p, c.host, msys2));
    g.checks.push_back(host_cxx_check(p, c.host));
    r.groups.push_back(g);
}

} // namespace nnh::tool
