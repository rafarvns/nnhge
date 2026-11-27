// Which engine a new project uses (engine_location.hpp).

#include "new/engine_location.hpp"

#include <filesystem>

#include "repo.hpp"
#include "text_file.hpp"

namespace nnh::tool {
namespace {

std::string without_trailing_slash(std::string path) {
    while (path.size() > 1 && path.back() == '/' && !(path.size() == 3 && path[1] == ':')) path.pop_back();
    return path;
}

std::string comparable(std::string path, bool windows) {
    for (char& c : path) {
        if (c == '\\') c = '/';
        if (windows && c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return without_trailing_slash(path);
}

bool same_folder(const std::string& a, const std::string& b, bool windows) {
    return comparable(a, windows) == comparable(b, windows);
}

/// The local engine's lines: which folder, why the variable, the set-lines unless it already names it.
void local_notes(EngineChoice& c, bool pinned, const char* env, bool windows) {
    c.notes.push_back(pinned ? "nnh: this project pins its engine release; the local engine " + c.local_dir + " (NNHGE " +
                                   c.local_version + ") replaces it while NNHGE_ENGINE_DIR names it."
                             : "nnh: this project uses the local engine " + c.local_dir + " (NNHGE " + c.local_version +
                                   "; no engine release is pinned: this nnh was built from a checkout).");
    const bool set = env != nullptr && *env != '\0';
    if (set && same_folder(env, c.local_dir, windows)) {
        c.notes.push_back("     NNHGE_ENGINE_DIR already names it.");
        return;
    }
    if (set) {
        c.notes.push_back("nnh: NNHGE_ENGINE_DIR is set to " + std::string(env) +
                          ": builds of this project will use that engine");
    }
    c.notes.push_back(pinned ? "     To build with it, set NNHGE_ENGINE_DIR in your shell:"
                             : "     Builds need NNHGE_ENGINE_DIR; set it in your shell:");
    for (const std::string& line : env_lines(c.local_dir, windows)) c.notes.push_back("       " + line);
}

} // namespace

EngineChoice choose_engine(const BuildInfo& info, const std::string& engine_option, const char* env_value, bool windows,
                           const EngineVersionOf& version_of) {
    EngineChoice c;
    if (info.release()) {
        c.engine_url = info.engine_url;
        c.engine_sha256 = info.engine_sha256;
    }
    if (!engine_option.empty()) {
        c.local_dir = engine_option;
        c.local_version = version_of(c.local_dir);
        if (c.local_version.empty()) {
            c.error = "--engine " + c.local_dir + " is not an NNHGE engine folder (no project(nnhge VERSION ...) in its "
                      "CMakeLists.txt, or no cmake/nnh_game.cmake)";
            return c;
        }
    } else if (!info.release()) {
        c.local_dir = info.checkout_dir;
        if (c.local_dir.empty()) {
            c.error = "this nnh knows no engine checkout: pass --engine <an NNHGE checkout>";
            return c;
        }
        c.local_version = version_of(c.local_dir);
        if (c.local_version.empty()) {
            c.error = "the engine checkout this nnh was built from (" + c.local_dir +
                      ") is not an NNHGE engine folder any more (moved?): pass --engine <an NNHGE checkout>";
            return c;
        }
    }
    c.ok = true;
    if (!c.local_dir.empty()) {
        local_notes(c, info.release(), env_value, windows);
    } else if (env_value != nullptr && *env_value != '\0') {
        c.notes.push_back("nnh: NNHGE_ENGINE_DIR is set to " + std::string(env_value) +
                          ": builds of this project will use that engine, not its pinned release");
    }
    return c;
}

std::string engine_dir_from_option(const std::string& option, const std::string& cwd, bool windows) {
    std::string path = windows ? native_path(option) : option;
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    std::filesystem::path p(path);
    const bool absolute = windows ? (path.size() >= 2 && path[1] == ':') : (!path.empty() && path[0] == '/');
    if (!absolute) p = std::filesystem::path(cwd) / p;
    std::string out = p.lexically_normal().generic_string();
    return without_trailing_slash(out);
}

std::vector<std::string> env_lines(const std::string& folder, bool windows) {
    const bool space = folder.find(' ') != std::string::npos;
    const std::string q = space ? "\"" + folder + "\"" : folder;
    if (!windows) {
        return {"export NNHGE_ENGINE_DIR=" + q, "(add that line to ~/.profile for every new shell)"};
    }
    return {"Git Bash:    export NNHGE_ENGINE_DIR=" + q,
            "PowerShell:  $env:NNHGE_ENGINE_DIR = \"" + folder + "\"",
            std::string("cmd:         ") + (space ? "set \"NNHGE_ENGINE_DIR=" + folder + "\"" : "set NNHGE_ENGINE_DIR=" + folder),
            "every new shell (Windows): setx NNHGE_ENGINE_DIR " + q};
}

std::string engine_version_from_cmakelists(std::string_view text) {
    static constexpr std::string_view kStart = "project(nnhge VERSION ";
    std::size_t line = 0;
    while (line < text.size()) {
        std::size_t end = text.find('\n', line);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view l = text.substr(line, end - line);
        if (l.substr(0, kStart.size()) == kStart) {
            std::string v;
            int dots = 0;
            bool digit = false;
            for (char ch : l.substr(kStart.size())) {
                if (ch >= '0' && ch <= '9') {
                    digit = true;
                } else if (ch == '.' && digit) {
                    ++dots;
                    digit = false;
                } else {
                    break;
                }
                v += ch;
            }
            return dots == 2 && digit ? v : "";
        }
        line = end + 1;
    }
    return "";
}

std::string engine_folder_version(const std::string& dir) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(std::filesystem::path(dir) / "cmake" / "nnh_game.cmake", ec)) return "";
    return engine_version_from_cmakelists(read_text_file(dir + "/CMakeLists.txt", 64u * 1024u));
}

} // namespace nnh::tool
