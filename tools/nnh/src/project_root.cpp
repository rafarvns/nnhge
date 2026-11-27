#include "project_root.hpp"

#include <filesystem>

namespace nnh::tool {

namespace fs = std::filesystem;

ProjectRoot find_project_root(const std::string& start) {
    std::error_code ec;
    fs::path at = fs::absolute(start, ec);
    for (int depth = 0; depth < 64 && !at.empty(); ++depth) {
        if (fs::is_regular_file(at / "nnh.toml", ec)) return {at.generic_string(), true};
        if (fs::exists(at / "CMakePresets.json", ec) && fs::is_directory(at / "engine" / "core", ec)) {
            return {at.generic_string(), false};
        }
        const fs::path parent = at.parent_path();
        if (parent == at) break;
        at = parent;
    }
    return {};
}

std::string no_project_message() {
    return "nnh: no game project here: no nnh.toml (game project) or NNHGE repository (CMakePresets.json + "
           "engine/core) in this folder or above it";
}

} // namespace nnh::tool
