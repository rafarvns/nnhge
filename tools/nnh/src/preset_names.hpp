#pragma once
// Does a project define the preset nnh is about to use (spec 032 FR-13)? nnh runs `cmake --preset <p>` and
// `cmake --build --preset <p>`, so both a configure preset and a build preset of that name must exist, in
// CMakePresets.json or CMakeUserPresets.json. Checked before CMake runs, so a game project without the six presets
// gets a message naming the preset instead of CMake's. Pure (a bounded JSON reader: 1 MiB, 64 levels; no comments, as
// CMake allows none); tested in tools/nnh/tests/test_preset_names.cpp.

#include <string>
#include <string_view>

namespace nnh::tool {

inline constexpr std::size_t kMaxPresetsBytes = 1u << 20;

/// True when the two files' texts together have a configure preset and a build preset named `preset` (the "name"
/// member of an object directly in the top-level "configurePresets" / "buildPresets" arrays). A malformed or oversized
/// text defines nothing. Either text may be empty (no such file).
bool presets_define(std::string_view presets_json, std::string_view user_presets_json, const std::string& preset);

/// presets_define over files (a missing file is empty).
bool presets_file_defines(const std::string& presets_path, const std::string& user_presets_path,
                          const std::string& preset);

} // namespace nnh::tool
