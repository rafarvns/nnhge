#pragma once
// The .nnhmesh reader's checks (spec 026 FR-7, constitution X): never trusts the file. Pure; shared with nnh-assetc's
// self-check and fuzzed (tests/fuzz/fuzz_mesh.cpp).

#include <cstddef>
#include <cstdint>

#include "mesh/mesh_format.hpp"

namespace nnh::detail::mesh {

/// A validated file, in place (the caller keeps the bytes alive).
struct MeshView {
    MeshHeader header{};                        ///< a copy
    const std::uint8_t* submeshes = nullptr;    ///< header.submesh_count SubmeshRecords (read with memcpy)
    const char* textures = nullptr;             ///< header.texture_count paths of kPathBytes, NUL-terminated
    const std::uint8_t* vertices = nullptr;
    const std::uint8_t* indices = nullptr;      ///< u16 each (read with memcpy)
};

/// Checks magic, version, sizes, the exact layout, CRC, limits, stride against flags, finite and positive scales,
/// bounds, every index < vertex count, each submesh's range and material, texture paths. @return false and a reason
/// in `why` (at least 64 bytes) when anything is off.
bool validate(const void* data, std::size_t size, MeshView& out, char* why, std::size_t why_size);

/// Submesh `i` of a validated view.
SubmeshRecord submesh(const MeshView& view, int i);

} // namespace nnh::detail::mesh
