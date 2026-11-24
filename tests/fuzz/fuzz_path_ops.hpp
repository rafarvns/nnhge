#pragma once
// Fuzz target for pathfinding (spec 025 T6.3): kept in its own header (not fuzz_targets.hpp) so parallel specs never
// touch the same lines.

#include <cstddef>
#include <cstdint>

namespace nnh::fuzz {

/// Bytes -> a map (size, settings, cells), then a stream of syncs, edits, searches (one-shot and sliced, with edits
/// between slices), smoothings, field builds and queries, follower steps; aborts when a found path is invalid, a sliced
/// search differs from find(), or a waypoint or field answer breaks its contract. Out-of-range values (misuse) only in
/// Release builds, where they must be harmless.
int path_ops_target(const std::uint8_t* data, std::size_t size);

} // namespace nnh::fuzz
