#pragma once
// The texture kind of hot reload (spec 031, texture_reload.cpp): called from textures.cpp and gfx_frame.cpp in
// Debug/Development branches only. Tests: tests/gfx/test_texture_reload.cpp.

#include <cstdint>

namespace nnh {
class Texture;
}

namespace nnh::detail {

/// Non-null while platform textures replaced by a reload wait for the GPU to be idle (gfx_gpu_idle calls it).
extern void (*g_texture_retire_hook)();

/// load_texture() made slot `slot` (generation `generation`) from the asset `asset_path`: the slot reloads with it.
/// Registers the kind (and its shutdown hook) the first time.
void texture_reload_loaded(int slot, std::uint32_t generation, const char* asset_path);

/// The size in the slot (a reload may have changed it), or the size cached at load for a released texture.
int texture_width_dev(const Texture& t);
int texture_height_dev(const Texture& t);

} // namespace nnh::detail
