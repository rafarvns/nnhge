#pragma once
// The dump (spec 030 FR-14, plan "Dump", question 6): every tunable that differs from its default as TOML in spec 014's
// subset — ungrouped names first, then one table per group path in byte order (`[fx.spark]`), each line
// `leaf = value # default D` — between `dump: begin` and `dump: end` lines. Tests: tests/live/test_live_dump.cpp.

#include <cstdint>

#include "live/live_block.hpp"

namespace nnh::detail::live {

/// Writes the dump's lines (each starting with `dump: `, NUL-terminated, without a newline) through `emit`.
/// @return the number of changed tunables.
int write_dump(const Registry& r, std::uint32_t frame, void (*emit)(void* context, const char* line), void* context);

/// The text of an entry's value bits as a dump writes it (`5.25`, `true`, `"#FFC040FF"`, `"fire"`): `out` 48 bytes.
int dump_value(const Entry& e, std::uint32_t bits, char out[48]);

} // namespace nnh::detail::live
