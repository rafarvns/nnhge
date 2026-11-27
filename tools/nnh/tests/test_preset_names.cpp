// Does a presets file define a preset (preset_names.hpp, spec 032 FR-13): a configure and a build preset of that name.

#include <string>

#include "preset_names.hpp"
#include "test.hpp"

using namespace nnh::tool;

namespace {

const char* const kRepoStyle = R"({
  "version": 6,
  "configurePresets": [
    { "name": "base", "hidden": true, "generator": "Ninja", "binaryDir": "${sourceDir}/build/${presetName}" },
    { "name": "pc-debug", "inherits": ["base"], "cacheVariables": { "X": "$env{NNHGE_ENGINE_DIR}" } },
    { "name": "3ds-dev", "inherits": "base" }
  ],
  "buildPresets": [ { "name": "pc-debug", "configurePreset": "pc-debug" },
                    { "name": "3ds-dev", "configurePreset": "3ds-dev" } ],
  "testPresets": [ { "name": "pc-debug", "configurePreset": "pc-debug" } ]
})";

} // namespace

NNH_TEST(presets_of_the_repository_style_file_are_found) {
    CHECK(presets_define(kRepoStyle, "", "pc-debug"));
    CHECK(presets_define(kRepoStyle, "", "3ds-dev"));
    CHECK(!presets_define(kRepoStyle, "", "3ds-release"));
    CHECK(!presets_define(kRepoStyle, "", "base")); // configure only, no build preset
}

NNH_TEST(the_real_repository_presets_define_the_six) {
    // The engine's own file, read by the test (NNH_TOOL_SOURCE_DIR = tools/nnh).
    CHECK(presets_file_defines(std::string(NNH_TOOL_SOURCE_DIR) + "/../../CMakePresets.json", "", "3ds-release"));
    CHECK(presets_file_defines(std::string(NNH_TOOL_SOURCE_DIR) + "/../../CMakePresets.json", "", "pc-debug"));
    CHECK(!presets_file_defines(std::string(NNH_TOOL_SOURCE_DIR) + "/no-such-file.json", "", "pc-debug"));
}

NNH_TEST(a_user_presets_file_adds_presets) {
    const char* user = R"({ "version": 6, "configurePresets": [ { "name": "pc-mine" } ],
                           "buildPresets": [ { "name": "pc-mine", "configurePreset": "pc-mine" } ] })";
    CHECK(presets_define(kRepoStyle, user, "pc-mine"));
    CHECK(presets_define("", user, "pc-mine"));
    CHECK(!presets_define(kRepoStyle, user, "pc-other"));
}

NNH_TEST(escaped_strings_are_read) {
    const char* text = R"({ "configurePresets": [ { "displayName": "a \"quoted\" name", "name": "pc-dev" } ],
                            "buildPresets": [ { "name": "pc-dev", "description": "back\\slash" } ] })";
    CHECK(presets_define(text, "", "pc-dev"));
}

NNH_TEST(a_preset_only_in_build_presets_is_not_found) {
    const char* text = R"({ "configurePresets": [], "buildPresets": [ { "name": "pc-dev" } ] })";
    CHECK(!presets_define(text, "", "pc-dev"));
}

NNH_TEST(a_name_outside_the_preset_arrays_does_not_count) {
    const char* text = R"({ "vendor": { "configurePresets": [ { "name": "pc-dev" } ] },
                            "configurePresets": [ { "cacheVariables": { "name": "pc-dev" } } ],
                            "buildPresets": [ { "name": "pc-dev" } ] })";
    CHECK(!presets_define(text, "", "pc-dev"));
}

NNH_TEST(malformed_json_finds_nothing) {
    CHECK(!presets_define(R"({ "configurePresets": [ { "name": "pc-dev" )", "", "pc-dev"));
    CHECK(!presets_define(R"({ "configurePresets": [ { "name": "pc-dev" } ], "buildPresets": [ { "name": "pc-dev" } ] } x)",
                          "", "pc-dev"));
    CHECK(!presets_define("[[[[", "", "pc-dev"));
    CHECK(!presets_define(std::string(10000, '['), "", "pc-dev")); // too deep: refused, no recursion overflow
}

NNH_TEST(a_presets_file_over_1_mib_is_refused) {
    std::string big = kRepoStyle;
    big.append(1u << 20, ' ');
    CHECK(!presets_define(big, "", "pc-debug"));
}
