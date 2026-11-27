#pragma once
// PC simulator: the Debug/Development data folder next to the program (pure, SDL-free; tests/platform_conformance/
// test_data_root_pc.cpp). It lives under `nnh_user/`, never `<exe dir>/<anything the build makes>/`: on Linux the program
// itself is the file `<exe dir>/<game>`, so `<exe dir>/data/data/` (the `data` example) could not be created (spec 031,
// a Linux finding of hot reload's received files).

#include <cstddef>

namespace nnh::platform::pc {

/// "<base>nnh_user/<folder>/" into `out` (`base` ends with '/'). @return false when it doesn't fit (nothing cut).
bool dev_data_root(const char* base, const char* folder, char* out, std::size_t size);

} // namespace nnh::platform::pc
