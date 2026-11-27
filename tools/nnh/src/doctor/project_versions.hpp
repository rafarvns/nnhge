#pragma once
// What nnh doctor reads from a project's files (spec 033 FR-19): the engine version a configure recorded in a
// CMake cache, and the engine pin of a CMakeLists.txt in the form `nnh new` writes. Bounded (the caller reads
// at most 4 MiB) and pure; tested in tools/nnh/tests/test_doctor_project.cpp and mini-fuzzed in
// test_fuzz_tool.cpp.

#include <string>
#include <string_view>

namespace nnh::tool {

/// "x.y.z" of the line `NNHGE_VERSION:<type>=x.y.z` of a CMakeCache.txt, else "".
std::string cache_engine_version(std::string_view cache);

/// The URL of the line `set(nnhge_url "<url>")` (the form nnh new writes), else "" (none, empty, or another
/// form).
std::string project_pin_url(std::string_view cmakelists);

} // namespace nnh::tool
