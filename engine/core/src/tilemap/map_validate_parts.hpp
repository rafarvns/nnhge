#pragma once
// Internal to map_validate*.cpp: a reader over a file whose header, layout and counts were already checked, and
// the checks of the later sections. Reads through memcpy, so the file may have any alignment.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "tilemap/map_format.hpp"
#include "tilemap/map_validate.hpp"

namespace nnh::detail::mapcheck {

inline MapCheck damaged(const char* reason) { return {MapProblem::Damaged, reason}; }
inline MapCheck newer(const char* reason) { return {MapProblem::NewerVersion, reason}; }
inline MapCheck over(const char* reason) { return {MapProblem::OverLimit, reason}; }

inline bool finite(float f) { return std::isfinite(f); }

/// Entry `index` of a section, or the u16 at `offset`: callers keep indices below the checked counts.
struct Reader {
    const std::uint8_t* data = nullptr;
    mapfmt::Header h{};

    template <typename T>
    T at(mapfmt::Section s, int index) const {
        T value;
        std::memcpy(&value, data + h.offsets[s] + static_cast<std::size_t>(index) * sizeof(T), sizeof value);
        return value;
    }
    std::uint16_t u16(std::uint32_t offset) const {
        std::uint16_t value;
        std::memcpy(&value, data + offset, sizeof value);
        return value;
    }
    const char* pool() const { return reinterpret_cast<const char*>(data + h.offsets[mapfmt::kStrings]); }
    /// A string offset is fine when it starts inside the used pool (which ends with a NUL: checked first).
    bool string_ok(std::uint32_t offset) const { return offset < h.string_bytes; }
    /// Index of the tileset holding `id` (1..tile_ids: checked by the caller).
    int tileset_of(int id) const;
};

MapCheck check_tile_layers(const Reader& r);
MapCheck check_objects(const Reader& r);
MapCheck check_properties(const Reader& r);

} // namespace nnh::detail::mapcheck
