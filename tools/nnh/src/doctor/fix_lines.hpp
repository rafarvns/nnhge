#pragma once
// The `fix:` line of every problem nnh doctor reports, per host (spec 033 FR-20, D56 item 8): one command to
// paste, or one link when installing needs an official installer. On Windows a command works alike in
// PowerShell, cmd and Git Bash: forward-slash paths, no quoted program, double quotes only around arguments.
// One table, so the wording and the package names live in one place; tested through the checks
// (test_doctor_*.cpp).

#include <string>
#include <string_view>

namespace nnh::tool {

enum class Host { Windows, Debian, OtherLinux, MacOs };

/// Debian, Ubuntu and their derivatives (ID or ID_LIKE in /etc/os-release) get apt lines; other Linux systems
/// links.
Host linux_host(std::string_view os_release);

enum class Problem {
    InstallCmake,
    OldCmake,
    InstallNinja,
    InstallCompiler,
    OldCompiler,
    MsvcBestEffort,
    MacBestEffort,
    LinuxPkgConfig,
    LinuxWindowSystem,
    LinuxAudio,
    InstallDevkitPro,
    SetDevkitPro,
    Install3dsLibraries,
    DevkitProShellTools,
    HostCompilerFor3ds,
    InstallCcache,
    InstallGit,
    InstallAzahar,
    NoConsole,
    CaCertificates,
    Unreachable,
    ProjectFile,
    EngineVersion,
    UnknownEngineVersion,
    Presets,
    EngineDirNotEngine,
    EngineDirOverridesPin,
    DiskSpace,
};

/// The fix for `problem` on `host`. `detail`: the devkitPro MSYS2 folder (pacman lines on Windows), the CA
/// bundle found (CaCertificates), the host (Unreachable), "<project's version>|<nnh's version>"
/// (EngineVersion).
std::string fix_for(Problem problem, Host host, const std::string& detail = "");

/// The engine's docs page `path` ("guides/game-projects.md") on GitHub.
std::string docs_link(const char* path);

} // namespace nnh::tool
