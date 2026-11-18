#pragma once
// The 3DS texture size rule, enforced on every system (decision D4). Pure; unit tested in
// tests/gfx/test_texture_rules.cpp.

namespace nnh::detail {

inline constexpr int kMinTextureSide = 8;
inline constexpr int kMaxTextureSide = 1024;

/// True for a power of two from 8 to 1024.
constexpr bool valid_texture_side(int side) {
    return side >= kMinTextureSide && side <= kMaxTextureSide && (side & (side - 1)) == 0;
}

/// True when both sides are powers of two from 8 to 1024 (the PICA200's limits).
constexpr bool valid_texture_size(int width, int height) {
    return valid_texture_side(width) && valid_texture_side(height);
}

} // namespace nnh::detail
