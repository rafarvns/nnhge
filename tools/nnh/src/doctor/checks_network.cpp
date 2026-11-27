// nnh doctor's Console and Downloads groups (checks.hpp): the only ones that touch the network, and only with
// --console / --online (FR-18). Downloads fetch one byte of each pinned URL through the CMake a build would
// use, with the CA bundle cmake/nnh_sdl3.cmake would give it, so a CMake without CA certificates shows here
// first.

#include "doctor/checks.hpp"
#include "doctor/project_versions.hpp"

namespace nnh::tool {
namespace {

/// A URL safe to write into the fetch script: https:// or file:// and no character CMake or the shell would
/// read.
bool safe_url(const std::string& url) {
    if (url.rfind("https://", 0) != 0 && url.rfind("file://", 0) != 0) return false;
    for (char c : url) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!plain && std::string("-._~:/?#[]@!&'()*+,=%").find(c) == std::string::npos) return false;
    }
    return url.size() <= 2048;
}

/// Git for Windows' CA bundle, which cmake/nnh_sdl3.cmake hands to CMake on Windows ("" when absent or
/// elsewhere).
std::string git_ca_bundle(const Probes& p, Host host) {
    if (host != Host::Windows) return "";
    std::string program_files = p.env("ProgramFiles");
    for (char& c : program_files)
        c = c == '\\' ? '/' : c;
    if (program_files.empty()) program_files = "C:/Program Files";
    for (const char* rest :
         {"/Git/mingw64/etc/ssl/certs/ca-bundle.crt", "/Git/usr/ssl/certs/ca-bundle.crt"}) {
        if (p.is_file(program_files + rest)) return program_files + rest;
    }
    return "";
}

/// What the build's CMake gets as CMAKE_TLS_CAINFO: the variable when set, else (Windows) Git's bundle.
std::string ca_bundle(const Probes& p, Host host) {
    const std::string set = p.env("CMAKE_TLS_CAINFO");
    return set.empty() ? git_ca_bundle(p, host) : set;
}

/// The SDL3 package a first PC configure downloads, or "" with a system SDL3 (Linux) or none to fetch.
std::string sdl3_url(const Probes& p, Host host) {
    if (host == Host::Windows) return kSdl3MingwUrl;
    if (host != Host::MacOs) {
        const std::string pkg = p.find_program("pkg-config");
        if (!pkg.empty()) {
            const RunResult r = p.run(pkg, {"--modversion", "sdl3"});
            if (r.started && r.exit_code == 0) return "";
        }
    }
    return kSdl3SourceUrl;
}

DoctorCheck fetch_check(const Probes& p, Host host, const char* name, const std::string& cmake,
                        const std::string& url) {
    const std::string cainfo = ca_bundle(p, host);
    const std::string error = p.fetch(cmake, url, cainfo);
    if (error.empty()) return {CheckState::Ok, name, url, ""};
    const bool certificate = error.find("certificate") != std::string::npos || error.rfind("60;", 0) == 0 ||
                             error.rfind("77;", 0) == 0;
    const std::size_t host_start = url.find("://") + 3;
    const std::string server = url.substr(host_start, url.find('/', host_start) - host_start);
    return {CheckState::Fail, name, url + ": " + error,
            certificate ? fix_for(Problem::CaCertificates, host, git_ca_bundle(p, host))
                        : fix_for(Problem::Unreachable, host, server)};
}

} // namespace

void check_console(const Probes& p, const DoctorContext& c, DoctorReport& r) {
    DoctorGroup g{"Console", true, "", {}};
    if (c.console.empty() && !c.online) {
        g.not_checked = "--console <ip> or --online";
        r.groups.push_back(g);
        return;
    }
    const std::string found = p.find_console(c.console);
    if (found.empty()) {
        g.checks.push_back(
            {CheckState::Fail, "console",
             c.console.empty() ? "none answered on the local network" : c.console + " did not answer",
             fix_for(Problem::NoConsole, c.host)});
    } else {
        g.checks.push_back({CheckState::Ok, "console", found + " answered", ""});
    }
    r.groups.push_back(g);
}

void check_downloads(const Probes& p, const DoctorContext& c, DoctorReport& r) {
    DoctorGroup g{"Downloads", true, "", {}};
    if (!c.online) {
        g.not_checked = "--online";
        r.groups.push_back(g);
        return;
    }
    const std::string cmake = p.find_program("cmake");
    if (cmake.empty()) {
        g.checks.push_back({CheckState::Fail, "cmake", "not found on PATH: the downloads go through it",
                            fix_for(Problem::InstallCmake, c.host)});
        r.groups.push_back(g);
        return;
    }
    if (const std::string sdl3 = sdl3_url(p, c.host); !sdl3.empty()) {
        g.checks.push_back(fetch_check(p, c.host, "SDL3", cmake, sdl3));
    } else {
        g.checks.push_back({CheckState::Ok, "SDL3", "system SDL3: nothing to download", ""});
    }
    if (c.game_project && !c.project_root.empty()) {
        const std::string pin = project_pin_url(p.read_file(c.project_root + "/CMakeLists.txt", 64u * 1024u));
        if (safe_url(pin)) g.checks.push_back(fetch_check(p, c.host, "engine", cmake, pin));
    }
    r.groups.push_back(g);
}

} // namespace nnh::tool
