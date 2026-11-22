#pragma once
// A test-only `.nnhsprite` writer (spec 016 T6.1): the block of a code-built library as file bytes (serial 0, CRC
// set), and helpers to corrupt one record and reseal the CRC.

#include <cstdint>
#include <cstring>
#include <vector>

#include "animation/sprite_format.hpp"
#include "nnh/animation.hpp"

namespace anim_test {

/// The library's block as a file.
std::vector<std::uint8_t> sprite_file(const nnh::SpriteLibrary& library);

/// A library with every kind of record: 6 frames (one trimmed), clips of every mode and direction, markers, 3 states
/// with next-on-finish and 3 rules.
nnh::SpriteLibrary rich_library();

/// Recomputes the CRC after an edit.
void reseal(std::vector<std::uint8_t>& file);

/// Record `i` of section `s`, to read or edit (then reseal()).
template <typename T>
T get(const std::vector<std::uint8_t>& file, int s, int i) {
    nnh::detail::spritefmt::Header h;
    std::memcpy(&h, file.data(), sizeof h);
    const std::uint32_t base = s == 0 ? sizeof h : h.offset[s - 1];
    T out;
    std::memcpy(&out, file.data() + base + static_cast<std::size_t>(i) * sizeof(T), sizeof(T));
    return out;
}
template <typename T>
void put(std::vector<std::uint8_t>& file, int s, int i, const T& value) {
    nnh::detail::spritefmt::Header h;
    std::memcpy(&h, file.data(), sizeof h);
    const std::uint32_t base = s == 0 ? sizeof h : h.offset[s - 1];
    std::memcpy(file.data() + base + static_cast<std::size_t>(i) * sizeof(T), &value, sizeof(T));
}

inline nnh::detail::spritefmt::Header header_of(const std::vector<std::uint8_t>& file) {
    nnh::detail::spritefmt::Header h;
    std::memcpy(&h, file.data(), sizeof h);
    return h;
}
inline void set_header(std::vector<std::uint8_t>& file, const nnh::detail::spritefmt::Header& h) {
    std::memcpy(file.data(), &h, sizeof h);
}

} // namespace anim_test
