#pragma once
// Which engine a new project uses (spec 033 FR-9, FR-10, D56 item 3; D55 item 8): a release nnh writes its pin; a
// checkout nnh writes no pin (the guard) and points the user at NNHGE_ENGINE_DIR, its own checkout by default or
// --engine. The folder is checked before anything is created, and its path is written into no file: nnh new prints the
// lines that set the variable. Pure apart from engine_folder_version; tested in
// tools/nnh/tests/test_engine_location.cpp.

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "build_info.hpp"

namespace nnh::tool {

struct EngineChoice {
    bool ok = false;
    std::string error;                ///< when !ok, without "nnh: "
    std::string engine_url;           ///< the pin written into CMakeLists.txt ("" = none: the guard)
    std::string engine_sha256;
    std::string local_dir;            ///< the local engine (drive form on Windows), "" = none
    std::string local_version;        ///< its version
    std::vector<std::string> notes;   ///< lines nnh new prints after the project is written
};

/// "" when `dir` is not an engine folder, else its version (dir -> engine_folder_version in nnh).
using EngineVersionOf = std::function<std::string(const std::string& dir)>;

/// `engine_option` is --engine already through engine_dir_from_option ("" = not given); `env_value` NNHGE_ENGINE_DIR
/// (null or "" = unset); `windows` picks the shells of the set-lines and how folders compare.
EngineChoice choose_engine(const BuildInfo& info, const std::string& engine_option, const char* env_value, bool windows,
                           const EngineVersionOf& version_of);

/// --engine as typed -> an absolute folder: MSYS "/e/x" -> "E:/x", '\' -> '/', relative to `cwd`, no trailing '/'.
std::string engine_dir_from_option(const std::string& option, const std::string& cwd, bool windows);

/// The lines that set NNHGE_ENGINE_DIR to `folder`: Git Bash, PowerShell, cmd and setx on Windows; export and the
/// ~/.profile hint elsewhere. Quoted when the folder has a space.
std::vector<std::string> env_lines(const std::string& folder, bool windows);

/// "x.y.z" of the first `project(nnhge VERSION x.y.z` line of a top-level CMakeLists.txt, else "".
std::string engine_version_from_cmakelists(std::string_view text);

/// The version of the engine folder `dir` (its CMakeLists.txt's project(nnhge VERSION ...) and cmake/nnh_game.cmake
/// present), else "".
std::string engine_folder_version(const std::string& dir);

} // namespace nnh::tool
