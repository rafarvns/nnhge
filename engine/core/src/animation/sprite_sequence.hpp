#pragma once
// The order of shown frames (spec 016 FR-4), as pure functions. A clip of `n` frames plays positions of a cycle:
// L = n for forward and reverse, L = 2n − 2 for ping-pong (n ≥ 2; 1 for a one-frame clip). Position p shows clip
// index p < n ? p : 2n − 2 − p; reverse mirrors it (n − 1 − i). A finite clip shows a fixed number of positions:
// Once/Repeat k: n·k; ping-pong with P one-way passes: 1 + P·(n − 1) (P for a one-frame clip). Pure: nnh-assetc and
// the validator use it.

#include <cstdint>

namespace nnh::detail {

struct SpriteSequenceEnd {
    bool finite = false;
    std::uint16_t end_step = 0;  ///< the last shown position, within its cycle
    std::uint16_t end_cycle = 0; ///< the cycle it is in
};

/// Positions in one cycle.
int sprite_cycle_length(int frames, std::uint8_t mode);

/// The clip index shown at position `step` of the cycle.
int sprite_clip_index(int frames, std::uint8_t mode, std::uint8_t direction, int step);

/// Where a clip ends. `count`: Once 1, Repeat plays, PingPong passes (0 = forever), Loop 0.
SpriteSequenceEnd sprite_sequence_end(int frames, std::uint8_t mode, int count);

} // namespace nnh::detail
