// nnh doctor's Optional group (checks.hpp): ccache, git and the Azahar emulator; found or a warning, never a
// FAIL.

#include "doctor/checks.hpp"
#include "doctor/tool_versions.hpp"

namespace nnh::tool {
namespace {

/// `ask_version`: run `<program> --version` (not for a GUI program, which might open a window).
DoctorCheck optional_tool(const Probes& p, Host host, const char* name, const char* program, const char* why,
                          Problem install, bool ask_version = true) {
    const std::string path = p.find_program(program);
    if (path.empty())
        return {CheckState::Warn, name, std::string("not found (") + why + ")", fix_for(install, host)};
    const Version v = ask_version ? first_version(p.run(path, {"--version"}).output) : Version{};
    return {CheckState::Ok, name, (v.valid() ? v.text() : std::string("found")) + " (" + path + ")", ""};
}

} // namespace

void check_optional(const Probes& p, const DoctorContext& c, DoctorReport& r) {
    DoctorGroup g{"Optional", false, "", {}};
    g.checks.push_back(optional_tool(p, c.host, "ccache", "ccache", "rebuilds are slower without it",
                                     Problem::InstallCcache));
    g.checks.push_back(
        optional_tool(p, c.host, "git", "git", "to keep your game's history", Problem::InstallGit));
    g.checks.push_back(optional_tool(p, c.host, "Azahar", "azahar",
                                     "a 3DS emulator, to try .3dsx files on the PC", Problem::InstallAzahar,
                                     false));
    r.groups.push_back(g);
}

} // namespace nnh::tool
