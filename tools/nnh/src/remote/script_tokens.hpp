#pragma once
// The words of one script line and their values (spec 013, plan "Script language"): used by
// script_parser.cpp. Pure; tested through tools/nnh/tests/test_script_parser.cpp.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nnh::tool::remote {

struct Token {
    std::string text;    ///< unescaped for quoted tokens (\" → ", \\ → \, other escapes kept as written)
    bool quoted = false;
};

/// Splits a line into words and "quoted texts"; `#` outside quotes ends the line.
/// @return "" or the error ("unterminated quote", ...).
std::string tokenize(std::string_view line, std::vector<Token>& out);

/// A whole number of frames, 1…kMaxFrames.
bool parse_frames(std::string_view s, std::uint32_t& out);
/// A whole number in [0, max].
bool parse_index(std::string_view s, int max, std::int16_t& out);
/// A decimal in [−1, 1] with at most 3 decimals, as thousandths.
bool parse_stick_value(std::string_view s, std::int16_t& out);
/// Button names joined by '+' (case-insensitive). @return "" or the error.
std::string parse_buttons(std::string_view s, std::uint32_t& out);
/// NAME: 1…kMaxNameChars of A-Z a-z 0-9 _ -.
bool valid_name(std::string_view s);
/// Lower-case ASCII copy.
std::string lower(std::string_view s);

} // namespace nnh::tool::remote
