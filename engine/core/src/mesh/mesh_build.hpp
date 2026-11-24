#pragma once
// MeshDesc (floats, the game's or the converter's arrays) → .nnhmesh bytes (spec 026 FR-4/FR-5, D42). Pure and shared
// by create_mesh and nnh-assetc mesh, so a model built in code and the same data converted from glTF give the same
// bytes (AC-4). Never allocates: measure() validates and sizes, write() fills the caller's buffer. -ffp-contract=off.

#include <cstddef>
#include <cstdint>

#include "nnh/mesh.hpp"

namespace nnh::detail::mesh {

/// What only a converted model has: texture paths (asset paths without ".png") and each submesh's texture index.
struct BuildExtras {
    const char* const* texture_paths = nullptr;
    int texture_count = 0;
    const std::uint8_t* submesh_textures = nullptr; ///< submesh_count entries: an index or kNoTexture; null = none
};

struct BuildInfo {
    bool ok = false;
    char error[96] = {};     ///< why not, when !ok
    std::size_t size = 0;    ///< the file's size
    int vertex_count = 0;    ///< after unwelding (flat normals)
    int index_count = 0;
    int submesh_count = 0;
    bool flat = false;       ///< normals were generated per triangle
};

/// Validates `desc` (counts, indices, finite values, submesh ranges, materials, texture paths) and sizes the file.
BuildInfo measure(const MeshDesc& desc, const BuildExtras& extras = {});

/// Writes the file measure() sized (`size` = info.size) into `out`. @return false when `desc` doesn't measure ok.
bool write(const MeshDesc& desc, const BuildExtras& extras, std::uint8_t* out, std::size_t size);

/// A texture path the format accepts: 1..63 characters of [A-Za-z0-9_./-], no "..", no leading '/'.
bool valid_texture_path(const char* path);

} // namespace nnh::detail::mesh
