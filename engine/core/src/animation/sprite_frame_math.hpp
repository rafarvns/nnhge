#pragma once
// Pivots and offsets of frames (spec 016 FR-1, FR-10): default pivots in 1/16 pixel, and the pivot gfx::draw wants
// (in the trimmed region, mirrored by the flips). gfx::draw keeps the pivot fixed in the drawn quad and mirrors the
// texture inside it, so mirroring the pivot inside the region lands a trimmed frame where its untrimmed original
// would be, flipped or not.

#include <cstdint>

#include "animation/sprite_format.hpp"
#include "nnh/animation.hpp"

namespace nnh::detail {

struct SpritePivot16 {
    std::int16_t x, y; ///< 1/16 px
};

/// The default pivot of an untrimmed frame of `w`×`h` pixels (bottom-centre of an odd width is exact: x.5).
inline SpritePivot16 sprite_default_pivot(std::uint8_t pivot, int w, int h) {
    switch (pivot) {
    case spritefmt::kCenter: return {static_cast<std::int16_t>(w * 8), static_cast<std::int16_t>(h * 8)};
    case spritefmt::kBottomCenter: return {static_cast<std::int16_t>(w * 8), static_cast<std::int16_t>(h * 16)};
    default: return {0, 0};
    }
}

inline SpriteFrame sprite_frame_of(const spritefmt::FrameEntry& e) {
    return {{e.x, e.y, e.w, e.h},
            {static_cast<float>(e.pivot_x) / 16.0f, static_cast<float>(e.pivot_y) / 16.0f},
            {static_cast<float>(e.offset_x), static_cast<float>(e.offset_y)}};
}

/// The pivot in the region's pixels: the untrimmed pivot minus the trim offset, mirrored when flipped.
inline Vec2 sprite_draw_pivot(const spritefmt::FrameEntry& e, bool flip_x, bool flip_y) {
    int x16 = e.pivot_x - e.offset_x * 16;
    int y16 = e.pivot_y - e.offset_y * 16;
    if (flip_x) x16 = e.w * 16 - x16;
    if (flip_y) y16 = e.h * 16 - y16;
    return {static_cast<float>(x16) / 16.0f, static_cast<float>(y16) / 16.0f};
}

} // namespace nnh::detail
