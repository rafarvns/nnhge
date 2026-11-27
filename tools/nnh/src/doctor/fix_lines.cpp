// The fix line of every problem nnh doctor reports (fix_lines.hpp). Package names and links checked
// 2026-11-27 (plan 033, stage 6 results).

#include "doctor/fix_lines.hpp"

namespace nnh::tool {
namespace {

/// Four forms: Windows, Debian-like Linux, other Linux, macOS.
std::string pick(Host host, const char* windows, const char* debian, const char* other_linux,
                 const char* mac) {
    switch (host) {
    case Host::Windows:
        return windows;
    case Host::Debian:
        return debian;
    case Host::OtherLinux:
        return other_linux;
    default:
        return mac;
    }
}

constexpr const char* kWinLibs = "winget install BrechtSanders.WinLibs.POSIX.UCRT";
constexpr const char* kDevkitProInstaller = "https://github.com/devkitPro/installer/releases";
constexpr const char* kDevkitProPacman = "https://devkitpro.org/wiki/devkitPro_pacman";
constexpr const char* kSdlLinux = "https://wiki.libsdl.org/SDL3/README-linux";

/// devkitPro's pacman: through its MSYS2 bash on Windows (`msys2` = its folder), dkp-pacman elsewhere.
std::string pacman(Host host, const std::string& msys2, const char* packages) {
    // A program path with a space would need quoting that PowerShell, cmd and Git Bash read differently: the page.
    if (host == Host::Windows && msys2.find(' ') != std::string::npos) return kDevkitProPacman;
    if (host == Host::Windows) return msys2 + "/usr/bin/bash.exe -lc \"pacman -S --needed " + packages + "\"";
    return std::string("sudo dkp-pacman -S --needed ") + packages;
}

} // namespace

Host linux_host(std::string_view os_release) {
    for (std::string_view key : {"\nID=", "\nID_LIKE="}) {
        const std::string text = "\n" + std::string(os_release);
        const std::size_t at = text.find(key);
        if (at == std::string::npos) continue;
        const std::string value = text.substr(at + key.size(), text.find('\n', at + 1) - at - key.size());
        if (value.find("debian") != std::string::npos || value.find("ubuntu") != std::string::npos)
            return Host::Debian;
    }
    return Host::OtherLinux;
}

std::string docs_link(const char* path) {
    return std::string("https://github.com/rafarvns/nnhge/blob/main/docs/") + path;
}

std::string fix_for(Problem problem, Host host, const std::string& detail) {
    switch (problem) {
    case Problem::InstallCmake:
        return pick(host, "winget install Kitware.CMake", "sudo apt install cmake",
                    "https://cmake.org/download/", "brew install cmake");
    case Problem::OldCmake:
        return "https://cmake.org/download/";
    case Problem::InstallNinja:
        return pick(host, "winget install Ninja-build.Ninja", "sudo apt install ninja-build",
                    "https://github.com/ninja-build/ninja/releases", "brew install ninja");
    case Problem::InstallCompiler:
    case Problem::HostCompilerFor3ds:
        return pick(host, kWinLibs, "sudo apt install g++", "https://gcc.gnu.org/install/",
                    "xcode-select --install");
    case Problem::OldCompiler:
        return pick(host, kWinLibs, "sudo apt install g++-13", "https://gcc.gnu.org/install/",
                    "xcode-select --install");
    case Problem::MsvcBestEffort:
        return kWinLibs;
    case Problem::MacBestEffort:
        return docs_link("guides/getting-started.md");
    case Problem::LinuxPkgConfig:
        return host == Host::Debian ? "sudo apt install pkg-config" : kSdlLinux;
    case Problem::LinuxWindowSystem:
        return host == Host::Debian
                   ? "sudo apt install libx11-dev libxext-dev libwayland-dev libxkbcommon-dev"
                   : kSdlLinux;
    case Problem::LinuxAudio:
        return host == Host::Debian ? "sudo apt install libasound2-dev libpulse-dev" : kSdlLinux;
    case Problem::InstallDevkitPro:
        return host == Host::Windows ? kDevkitProInstaller : kDevkitProPacman;
    case Problem::SetDevkitPro:
        return "export DEVKITPRO=/opt/devkitpro";
    case Problem::Install3dsLibraries:
        return pacman(host, detail, "3ds-dev");
    case Problem::DevkitProShellTools:
        return pacman(host, detail, "cmake ninja");
    case Problem::InstallCcache:
        return pick(host, "winget install Ccache.Ccache", "sudo apt install ccache",
                    "https://ccache.dev/download.html", "brew install ccache");
    case Problem::InstallGit:
        return pick(host, "winget install Git.Git", "sudo apt install git", "https://git-scm.com/downloads",
                    "xcode-select --install");
    case Problem::InstallAzahar:
        return "https://azahar-emu.org/";
    case Problem::NoConsole:
        return "open the Homebrew Launcher on the 3DS and press Y, then try again";
    case Problem::CaCertificates:
        if (host == Host::Debian) return "sudo apt install ca-certificates";
        if (host == Host::Windows && !detail.empty()) return "setx CMAKE_TLS_CAINFO \"" + detail + "\"";
        return "https://cmake.org/cmake/help/latest/variable/CMAKE_TLS_CAINFO.html";
    case Problem::Unreachable:
        return "check this computer's connection to " + detail + " (a proxy, a firewall)";
    case Problem::ProjectFile:
        return docs_link("guides/game-projects.md");
    case Problem::EngineVersion: {
        const std::size_t bar = detail.find('|');
        const std::string theirs = detail.substr(0, bar),
                          ours = bar == std::string::npos ? "" : detail.substr(bar + 1);
        return "use the nnh of NNHGE " + theirs + " (the game-tool protocol must match), or pin NNHGE " +
               ours + " in CMakeLists.txt";
    }
    case Problem::UnknownEngineVersion:
        return "build the project once (nnh build <game> --pc)";
    case Problem::Presets:
        return docs_link("guides/game-projects.md");
    case Problem::EngineDirNotEngine:
    case Problem::EngineDirOverridesPin:
        return docs_link("guides/game-projects.md#a-local-engine");
    case Problem::DiskSpace:
        return "free some space on this drive (a new project's builds take about 120 MB)";
    }
    return "";
}

} // namespace nnh::tool
