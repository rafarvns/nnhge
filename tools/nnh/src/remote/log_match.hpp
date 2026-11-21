#pragma once
// Matching the game's log lines in test scripts (spec 013, plan "Script language"): `expect` and `fail-on`
// patterns, and the frame and level read from an engine line's "[f<n> <t>s] <L> " prefix.
// Pure; tested in tools/nnh/tests/test_log_match.cpp.

#include <cstdint>
#include <string>
#include <string_view>

namespace nnh::tool::remote {

/// True if `pattern` matches somewhere in `line` (the whole received line, prefix included).
/// `*` matches any run of characters (also none), `?` exactly one; `\*`, `\?` and `\\` are those characters;
/// a `\` before anything else is itself. Plain text therefore means "contains". Allocates a little.
bool log_matches(std::string_view pattern, std::string_view line);

struct LineInfo {
    bool has_frame = false; ///< an engine line: frame and level are valid
    std::uint32_t frame = 0;
    char level = 0; ///< T D I W E F
};

/// Frame and level of an engine log line (engine/core/src/log/format.cpp). Any other text (a raw printf,
/// a crash report) has no frame: the runner gives it the last tick's frame.
LineInfo line_info(const std::string& line);

} // namespace nnh::tool::remote
