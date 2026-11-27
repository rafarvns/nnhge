#pragma once
// A new project's .cia unique ID (spec 033 FR-11): random in the homebrew range, from the operating system's
// random source (std::random_device: rand_s on MinGW-w64's libstdc++, /dev/urandom or getrandom on Linux).
// Tested in tools/nnh/tests/test_project_name.cpp.

#include <cstdint>

namespace nnh::tool {

/// Uniform in [kUniqueIdMin, kUniqueIdMax] (new/project_name.hpp).
std::uint32_t random_unique_id();

} // namespace nnh::tool
