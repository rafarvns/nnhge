#pragma once
// Watches (spec 030 FR-17, Q2): registered watches read only when shown or followed, and per-frame watches kept in a
// hash table and swept after 120 frames unseen. Tests: tests/live/test_live_watch.cpp.

#include <cstdint>

#include "live/live_block.hpp"

namespace nnh::detail::live {

/// A watch's current value: kind, bytes (4, 8 for Vec2, text 0..31). Registered watches read their variable now
/// (counted in g_watch_reads); per-frame watches give their last value.
struct WatchValue {
    Kind kind = Kind::None;
    std::uint8_t length = 0;
    std::uint8_t bytes[32] = {};
};
WatchValue read_watch(const WatchEntry& w);
WatchValue read_shown(const ShownEntry& s);

/// Per-frame watches not shown since `frame` − 120 are freed; run every 60 frames by the frame end.
void sweep_shown(Registry& r, std::uint32_t frame);

/// Registered watch reads (tests: none on frames where nothing shows or follows them, FR-19).
extern std::uint32_t g_watch_reads;

} // namespace nnh::detail::live
