#include "project_file.hpp"

#include "remote/tunes_file.hpp"

namespace nnh::tool {

ProjectFile read_project_file(std::string_view text) {
    ProjectFile out;
    if (text.size() > kMaxProjectFileBytes) {
        out.errors.push_back("nnh.toml: the file is larger than 64 KiB");
        return out;
    }
    const remote::TunesFile file = remote::read_tunes(text, "nnh.toml");
    out.errors = file.errors;
    if (!out.errors.empty()) return out;
    for (const remote::TuneValue& v : file.values) {
        if (v.name == "name") {
            out.name = v.value;
        } else if (v.name == "engine") {
            out.engine = v.value;
        } else {
            out.warnings.push_back("nnh.toml:" + std::to_string(v.line) + ": unknown key \"" + v.name + "\" (ignored)");
        }
    }
    return out;
}

} // namespace nnh::tool
