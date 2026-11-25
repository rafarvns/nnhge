#pragma once
// Test scripts (.nnhs) for the console remote (spec 013 FR-3, plan "Script language"): one command per line,
// parsed into steps before anything is built or sent. Pure; tested in tools/nnh/tests/test_script_parser.cpp.
//
//   # comment              wait N                 press B[+B…]          release B[+B…] | release all
//   hold B[+B…] N          tap B[+B…]             stick circle|cstick X Y   touch X Y   drag X Y N
//   untouch                screenshot NAME [eyes] expect "TEXT" N       fail-on "TEXT"        end
//   slider V|off           (spec 026: the 3D slider 0…1, or back to the script's default 0)

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nnh::tool::remote {

enum class Command : std::uint8_t {
    Wait, Press, Release, ReleaseAll, Hold, Tap, Stick, Touch, Drag, Untouch, Screenshot, Expect, FailOn, End,
    Slider, SliderOff,
};

/// One script line. Only the fields of its command are meaningful.
struct Step {
    Command command = Command::End;
    int line = 0;               ///< 1-based line in the file
    std::uint32_t buttons = 0;  ///< Press, Release, Hold, Tap: nnh::Button bits
    std::uint32_t frames = 0;   ///< Wait, Hold, Drag, Expect: 1…kMaxFrames
    std::uint8_t stick = 0;     ///< Stick: 0 Circle Pad, 1 C-Stick
    std::int16_t x = 0;         ///< Stick: thousandths −1000…1000; Touch, Drag: 0…319; Slider: thousandths 0…1000
    std::int16_t y = 0;         ///< Stick: thousandths, up; Touch, Drag: 0…239
    bool eyes = false;          ///< Screenshot: the top screen's right eye too (`eyes`, spec 026)
    std::string text;           ///< Screenshot: NAME; Expect, FailOn: the pattern (log_match.hpp)
};

struct Script {
    std::string file;
    int lines = 0;                     ///< lines in the file
    std::vector<Step> steps;           ///< always ends with one End (added at the end of the file if missing)
    std::vector<std::string> warnings; ///< "file:line: warning: …"
};

// Limits (plan 013).
inline constexpr std::size_t kMaxScriptBytes = 64 * 1024;
inline constexpr int kMaxScriptLines = 4096;
inline constexpr std::size_t kMaxLineChars = 256;
inline constexpr std::size_t kMaxTextChars = 128;
inline constexpr std::size_t kMaxNameChars = 32;
inline constexpr std::uint32_t kMaxFrames = 216000; ///< per command: one hour at 60 Hz
/// An input this close after an `expect` probably reaches the console after its frame (warning).
inline constexpr std::uint32_t kExpectMargin = 10;

// Button bits (nnh::Button, <nnh/input.hpp>; the console checks them against kButtonMask).
inline constexpr std::uint32_t kButtonZL = 1u << 6;
inline constexpr std::uint32_t kButtonZR = 1u << 7;
// Capability bits of the console's Hello (1 << nnh::Capability, <nnh/system.hpp>).
inline constexpr std::uint32_t kCapabilityCStick = 1u << 4;
inline constexpr std::uint32_t kCapabilityZButtons = 1u << 5;

/// What a parser remembers between lines (a file, or lines typed one by one in interactive mode).
struct ScriptParser {
    std::string file;
    bool touching = false;   ///< a touch is down (for drag)
    bool ended = false;      ///< `end` was seen
    int expect_line = 0;     ///< the last expect, 0 if none
    std::uint32_t since_expect = 0;
    bool warned = false;     ///< the warning for that expect was given
};

/// Parses one line (no '\n'), appending its step, if any, to `script.steps`, a warning to
/// `script.warnings`, or an error ("file:line: what") to `errors`.
void parse_script_line(ScriptParser& parser, std::string_view text, int line, Script& script,
                       std::vector<std::string>& errors);

struct ParsedScript {
    Script script;
    std::vector<std::string> errors; ///< every error of the file, in line order; empty = ok
};

/// Parses a whole script. Never stops at the first error, except for the size limits.
ParsedScript parse_script(std::string_view text, const std::string& file);

} // namespace nnh::tool::remote
