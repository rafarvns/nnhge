#pragma once
// The host C++ compiler nnh hands to a 3DS configure on Windows (spec 032 FR-14, D55 item 7): devkitPro's MSYS2 shell
// has none on its PATH, and 3DS builds compile host programs (nnh-assetc, data schema programs). Chosen like
// scripts/dkp.sh: NNH_HOST_CXX when set, else g++, c++ or clang++ on nnh's own PATH. Tested in
// tools/nnh/tests/test_configure_command.cpp.

#include <functional>
#include <string>

namespace nnh::tool {

/// `env_value` (the NNH_HOST_CXX environment variable, may be null) when non-empty, else the first of g++, c++,
/// clang++ that `find_on_path` finds (it returns the program's full path, or "" when absent). "" when none.
std::string find_host_cxx(const char* env_value, const std::function<std::string(const char*)>& find_on_path);

/// The program `name` on this process's PATH (with ".exe" on Windows), as a full path; "" when absent.
std::string find_on_path(const char* name);

/// find_host_cxx with the real environment and PATH.
std::string host_cxx();

} // namespace nnh::tool
