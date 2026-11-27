// nnh doctor's This project group (checks.hpp): nnh.toml, the engine version against this nnh's, the presets
// nnh uses, NNHGE_ENGINE_DIR and the free disk space. Reads only: nothing in the project is written.

#include "doctor/checks.hpp"
#include "doctor/doctor_limits.hpp"
#include "doctor/project_versions.hpp"
#include "new/engine_location.hpp"
#include "preset_names.hpp"
#include "project_file.hpp"

#include <cstdio>

namespace nnh::tool {
namespace {

constexpr std::size_t kMaxCacheBytes = 4u << 20;
const char* const kCachedPresets[] = {"pc-dev",  "pc-debug",  "pc-release",
                                      "3ds-dev", "3ds-debug", "3ds-release"};

DoctorCheck ok(const char* name, const std::string& found) {
    return {CheckState::Ok, name, found, ""};
}

DoctorCheck toml_check(const Probes& p, const DoctorContext& c, ProjectFile& file) {
    file = read_project_file(p.read_file(c.project_root + "/nnh.toml", kMaxProjectFileBytes));
    const std::string fix = fix_for(Problem::ProjectFile, c.host);
    if (!file.errors.empty()) return {CheckState::Fail, "nnh.toml", file.errors.front(), fix};
    if (!file.warnings.empty()) return {CheckState::Warn, "nnh.toml", file.warnings.front(), fix};
    return ok("nnh.toml", c.project_root + (file.name.empty() ? "" : " (" + file.name + ")"));
}

/// The engine version the project builds with: a configure's cache, else nnh.toml, else the repository's own.
DoctorCheck engine_check(const Probes& p, const DoctorContext& c, const ProjectFile& file) {
    std::string version, source;
    for (const char* preset : kCachedPresets) {
        const std::string cache = "build/" + std::string(preset) + "/CMakeCache.txt";
        version = cache_engine_version(p.read_file(c.project_root + "/" + cache, kMaxCacheBytes));
        if (!version.empty()) {
            source = cache;
            break;
        }
    }
    if (version.empty() && c.game_project && !file.engine.empty()) version = file.engine, source = "nnh.toml";
    if (version.empty() && !c.game_project) {
        version =
            engine_version_from_cmakelists(p.read_file(c.project_root + "/CMakeLists.txt", 64u * 1024u));
        source = "CMakeLists.txt";
    }
    if (version.empty()) {
        return {CheckState::Warn, "engine", "version unknown",
                fix_for(Problem::UnknownEngineVersion, c.host)};
    }
    const std::string found = version + " (" + source + "; this nnh: " + c.info.version + ")";
    if (version == c.info.version) return ok("engine", found);
    return {CheckState::Warn, "engine", found,
            fix_for(Problem::EngineVersion, c.host, version + "|" + c.info.version)};
}

DoctorCheck presets_check(const Probes& p, const DoctorContext& c) {
    std::vector<std::string> wanted = {"pc-dev", "pc-release", "3ds-dev", "3ds-release"};
    if (const std::string chosen = p.env("NNH_PRESET"); !chosen.empty()) wanted.push_back(chosen);
    const std::string presets = p.read_file(c.project_root + "/CMakePresets.json", kMaxPresetsBytes);
    const std::string user = p.read_file(c.project_root + "/CMakeUserPresets.json", kMaxPresetsBytes);
    std::string missing;
    for (const std::string& name : wanted) {
        if (!presets_define(presets, user, name)) missing += (missing.empty() ? "" : ", ") + name;
    }
    if (missing.empty()) return ok("presets", "pc-dev, pc-release, 3ds-dev, 3ds-release");
    return {CheckState::Fail, "presets", "no preset " + missing + " (CMakePresets.json)",
            fix_for(Problem::Presets, c.host)};
}

DoctorCheck engine_dir_check(const Probes& p, const DoctorContext& c) {
    const std::string dir = p.env("NNHGE_ENGINE_DIR");
    if (dir.empty()) {
        return ok("NNHGE_ENGINE_DIR", c.game_project ? "not set (the project's pinned engine)" : "not set");
    }
    const std::string version =
        p.is_file(dir + "/cmake/nnh_game.cmake")
            ? engine_version_from_cmakelists(p.read_file(dir + "/CMakeLists.txt", 64u * 1024u))
            : "";
    if (version.empty()) {
        return {CheckState::Fail, "NNHGE_ENGINE_DIR",
                dir + ": not an NNHGE engine folder" + drive_form_hint(dir, c.host),
                fix_for(Problem::EngineDirNotEngine, c.host)};
    }
    const std::string found = dir + " (NNHGE " + version + ")";
    if (c.game_project &&
        !project_pin_url(p.read_file(c.project_root + "/CMakeLists.txt", 64u * 1024u)).empty()) {
        return {CheckState::Warn, "NNHGE_ENGINE_DIR",
                found + ": this project builds with it, not its pinned release",
                fix_for(Problem::EngineDirOverridesPin, c.host)};
    }
    return ok("NNHGE_ENGINE_DIR", found);
}

DoctorCheck disk_check(const Probes& p, const DoctorContext& c) {
    const std::int64_t free = p.free_bytes(c.project_root);
    if (free < 0) return ok("disk", "free space unknown");
    char text[64];
    if (free >= 1000ll * 1000 * 1000)
        std::snprintf(text, sizeof text, "%.1f GB free", static_cast<double>(free) / 1e9);
    else
        std::snprintf(text, sizeof text, "%lld MB free", static_cast<long long>(free / 1000000));
    const std::string fix = fix_for(Problem::DiskSpace, c.host);
    if (free < limits::kDiskFailBytes) return {CheckState::Fail, "disk", text, fix};
    if (free < limits::kDiskWarnBytes) return {CheckState::Warn, "disk", text, fix};
    return ok("disk", text);
}

} // namespace

void check_project(const Probes& p, const DoctorContext& c, DoctorReport& r) {
    DoctorGroup g{"This project", true, "", {}};
    if (c.project_root.empty()) {
        g.not_checked = "not inside a game project or the engine repository";
        r.groups.push_back(g);
        return;
    }
    ProjectFile file;
    if (c.game_project) g.checks.push_back(toml_check(p, c, file));
    g.checks.push_back(engine_check(p, c, file));
    g.checks.push_back(presets_check(p, c));
    g.checks.push_back(engine_dir_check(p, c));
    g.checks.push_back(disk_check(p, c));
    r.groups.push_back(g);
}

} // namespace nnh::tool
