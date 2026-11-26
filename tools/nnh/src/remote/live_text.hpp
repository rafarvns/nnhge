#pragma once
// Live values typed on the PC (spec 030 FR-11, FR-13): parsed once into exact bits against the catalogue's entry, with
// refusals in plain words before anything is sent; expected values for expect-value and their comparison; the text of
// a received value (the game's own formatter). Pure; tested in tools/nnh/tests/test_live_text.cpp.

#include <cstdint>
#include <string>

#include "remote/live_catalog.hpp"

namespace nnh::tool::remote {

/// The bits a `set NAME VALUE` sends: "" and `bits`, or why it is refused (`player/jump: 12 is outside 0..10`).
std::string parse_set_value(const LiveItem& item, const std::string& text, std::uint32_t& bits);

/// `unknown name "player/jmp" (did you mean "player/jump"?)`.
std::string unknown_name(const LiveCatalog& catalog, const std::string& name);

/// A value to compare with (expect-value).
struct Expected {
    detail::live::Kind kind = detail::live::Kind::None;
    std::uint8_t bytes[32] = {};
    int length = 0;
};

enum class CompareOp : std::uint8_t { Eq, Ne, Lt, Le, Gt, Ge };

/// "" and `out`, or why `text` can't be compared with `item` (with ordered operators only for numbers and enums).
std::string parse_expected(const LiveItem& item, const std::string& text, CompareOp op, Expected& out);

/// Whether a received value passes `op` against `expected`, within `tolerance` (floats and Vec2; 0 = exact).
bool value_passes(const detail::remote::LiveValueRecord& value, CompareOp op, const Expected& expected,
                  double tolerance);

/// The text of a received value (`5.25`, `(12.5, 40)`, `"idle"`, `#FFC040FF`, an enum's name).
std::string value_text(const detail::remote::LiveValueRecord& value, const LiveItem* item);
/// The text of an entry's 32 bits (catalogue listings): `5.25`, `true`, `#FFC040FF`, `fire`.
std::string bits_text(const LiveItem& item, std::uint32_t bits);

} // namespace nnh::tool::remote
