#pragma once
// Bounded validation of a `.nnhsprite` file (spec 016 FR-16, T6.1; constitution X): once it accepts a file, every
// count, index, range, name and duration in it is consistent, so copying it into a library and playing any clip or
// state stays in bounds and terminates. Pure (no engine includes): nnh-assetc checks the files it writes with the
// same code. Unit tested in tests/animation/test_sprite_validate.cpp; fuzzed by tests/fuzz (sprite_library_target).
//
// Order (the first problem wins): header (size, magic, version, size field, CRC, capacities against the ceilings,
// section offsets, counts against capacities, default pivot, reserved bytes), frames, clips (names, hashes, frame
// ranges, modes, ends, cycle durations), clip frames (durations, sheet frames, marker runs), states, rules. The
// header's serial is a memory-only field and is ignored. Cost: one pass over the file.

#include <cstddef>
#include <cstdint>

namespace nnh::detail {

enum class SpriteProblem : std::uint8_t {
    None,
    Damaged,      ///< not a sprite library, truncated, CRC mismatch or inconsistent (ErrorCode::Corrupted)
    NewerVersion, ///< made by a newer engine (ErrorCode::Unsupported)
    OverLimit,    ///< a capacity over the documented ceilings (ErrorCode::LimitReached)
};

struct SpriteCheck {
    SpriteProblem problem = SpriteProblem::None;
    const char* reason = ""; ///< static text, "" when ok
    bool ok() const { return problem == SpriteProblem::None; }
};

/// Validates `size` bytes at `data` (any alignment; `data` may be null when `size` is 0). Never allocates; reads only
/// [data, data + size).
SpriteCheck validate_sprite_library(const void* data, std::size_t size);

/// The CRC the header must hold: CRC-32 of bytes [16, size) (magic, version, serial, size and the CRC itself are not
/// covered).
std::uint32_t sprite_file_crc(const void* data, std::size_t size);

} // namespace nnh::detail
