#pragma once
// The host C++ compiler nnh hands to a 3DS configure on Windows (spec 032 FR-14, D55 item 7): devkitPro's MSYS2 shell
// has none on its PATH, and 3DS builds compile host programs (nnh-assetc, data schema programs). Chosen like
// scripts/dkp.sh: NNH_HOST_CXX when set, else g++, c++ or clang++ on nnh's own PATH. Tested in
// tools/nnh/tests/test_configure_command.cpp.

#include <functional>
#include <string>

#include "repo.hpp"

namespace nnh::tool {

/// `env_value` (the NNH_HOST_CXX environment variable, may be null) when non-empty, else the first of g++, c++,
/// clang++ that `find_on_path` finds (it returns the program's full path, or "" when absent). "" when none.
std::string find_host_cxx(const char* env_value, const std::function<std::string(const char*)>& find_on_path);

/// The program `name` on this process's PATH (with ".exe" on Windows), as a full path; "" when absent.
std::string find_on_path(const char* name);

/// What a configure hands to devkitPro's MSYS2 CMake (repo.hpp ConfigureExtras): nothing unless `devkitpro_shell` (a 3DS
/// preset on Windows); then the host compiler (find_host_cxx over `host_env` and `find_on_path`) and, only in a game
/// project (`game`: an nnh.toml at the root), a non-empty `engine_env` (NNHGE_ENGINE_DIR). Pure.
ConfigureExtras configure_extras(bool game, bool devkitpro_shell, const char* engine_env, const char* host_env,
                                 const std::function<std::string(const char*)>& find_on_path);

} // namespace nnh::tool
