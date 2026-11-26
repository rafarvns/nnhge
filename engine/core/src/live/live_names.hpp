#pragma once
// Live value names (spec 030 plan, "Names"): the rules (in <nnh/live.hpp>, shared with the compile-time check), the
// leaf and group split, group hashes. Pure: no engine state, so `nnh` (tools/nnh) compiles this file too.
// Tests: tests/live/test_live_names.cpp.

#include <cstddef>
#include <cstdint>

#include "nnh/live.hpp"

namespace nnh::detail::live {

/// The rules of a name of `length` bytes.
inline live_name::Error check_name(const char* text, std::size_t length) { return live_name::check(text, length); }

/// The rules of a NUL-terminated name (reads at most kMaxNameBytes + 1 bytes).
live_name::Error check_name(const char* text);

/// A short English reason for an error ("has an empty part"); "" for None.
const char* name_error_text(live_name::Error error);

/// Group levels of a valid name: the number of '/' (0..3).
int group_depth(const char* text, std::size_t length);

/// Bytes of the group path at `level` (0-based) of a valid name: `fx/spark/big` level 0 → 2 (`fx`), level 1 → 8
/// (`fx/spark`). @return 0 when the name has no such level.
std::size_t group_length(const char* text, std::size_t length, int level);

/// The leaf: what follows the last '/' (the whole name without groups).
const char* leaf(const char* text, std::size_t length);

/// FNV-1a 32 of the group path at `level`, or 0 when there is none.
std::uint32_t group_hash(const char* text, std::size_t length, int level);

/// True when `prefix` (a group path) is `text`'s group at some level: `fx` is a group of `fx/spark/big`, `fx/s` isn't.
bool in_group(const char* text, std::size_t length, const char* prefix, std::size_t prefix_length);

} // namespace nnh::detail::live
