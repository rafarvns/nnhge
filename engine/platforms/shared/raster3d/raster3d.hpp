#pragma once
// The engine's CPU rasteriser (spec 026 FR-22/FR-23, decision D41): draws the 3D pass of the PC simulator and of the
// fake platform's pixel tests into an RGBA8 colour buffer and a float depth buffer. Deterministic: 28.4 fixed-point
// edges in int64 with the top-left fill rule, homogeneous clipping (near, far and a ±8 NDC guard band), perspective-
// correct attributes, FR-10's shading with the shared lighting tables, nearest/bilinear sampling with the three wrap
// modes, cut-out, 8-bit integer blending; a fixed sequence of float operations compiled with -ffp-contract=off -O2 in
// every flavour, so the same frame gives the same bytes on every host compiler. No SDL; no allocation.
//
// Files: vertex_stage.cpp (decode and transform), clip.cpp, raster.cpp (setup, coverage, depth), shade.cpp (FR-10),
// sample.cpp (textures), light_tables.cpp (shared with the 3DS backend).

#include <cstdint>

#include "light_tables.hpp"
#include "nnh/platform/video3d.hpp"

namespace nnh::raster3d {

/// Where a pass draws: `width × height` RGBA8 pixels (top row first) and as many depth values.
struct Target {
    std::uint8_t* rgba = nullptr;
    float* depth = nullptr;
    int width = 400;
    int height = 240;
};

/// A texture's pixels (RGBA8, top row first). Empty = untextured.
struct TextureView {
    const std::uint8_t* rgba = nullptr;
    int width = 0;
    int height = 0;
};

/// A mesh's data as the platform received it (MeshUpload).
struct MeshView {
    const std::uint8_t* vertices = nullptr;
    std::uint32_t vertex_count = 0;
    std::uint8_t stride = 12;
    std::uint8_t flags = 0;
    const std::uint16_t* indices = nullptr;
    std::uint32_t index_count = 0;
};

/// Fills the colour with `clear` and the depth with +1 (the far plane).
void clear(const Target& target, Color clear);
/// Fills only the depth with +1 (a pass over 2D already drawn).
void clear_depth(const Target& target);

/// Draws one submesh range. `pass` gives the projection and the view-space lights.
void draw(const Target& target, const platform::Pass3D& pass, const LightTables& tables, const platform::Draw3D& draw,
          const MeshView& mesh, const TextureView& texture);

// ---- Stages (internal, tested on their own) -----------------------------------------------------------------------

inline constexpr int kAttributes = 12; ///< view position xyz, normal xyz, uv, colour rgba

/// A vertex after the vertex stage: clip position and attributes.
struct ClipVertex {
    float clip[4];
    float attr[kAttributes];
};

/// Decodes vertex `index` of `mesh` and transforms it (vertex_stage.cpp).
ClipVertex vertex_stage(const platform::Pass3D& pass, const platform::Draw3D& draw, const MeshView& mesh,
                        std::uint32_t index);

/// Clips a triangle against near, far and the guard band (clip.cpp). Writes at most 9 vertices; @return the count
/// (0 = nothing left, else ≥ 3, a convex fan).
int clip_triangle(const ClipVertex in[3], ClipVertex out[9]);

/// A vertex on the screen: 28.4 fixed-point position, depth (NDC z), 1/w and attributes × 1/w.
struct ScreenVertex {
    std::int32_t x = 0;
    std::int32_t y = 0;
    float z = 0;
    float inv_w = 0;
    float attr[kAttributes];
};
ScreenVertex to_screen(const ClipVertex& v, int width, int height);

/// The pixel callback of rasterize: (x, y, depth, perspective-correct attributes).
struct PixelSink {
    void (*fn)(void* context, int x, int y, float z, const float* attr) = nullptr;
    void* context = nullptr;
};
/// Covers triangle (a, b, c) with the top-left rule, calling `sink` per covered pixel centre (raster.cpp).
/// `cull_back`: skip triangles clockwise in NDC (counter-clockwise on the y-down screen). @return pixels covered.
int rasterize(const ScreenVertex& a, const ScreenVertex& b, const ScreenVertex& c, bool cull_back, int width,
              int height, const PixelSink& sink);

/// A texel with the material's filter and wraps, 0..1 RGBA (sample.cpp).
void sample(const TextureView& texture, float u, float v, std::uint8_t filter, std::uint8_t wrap_u,
            std::uint8_t wrap_v, float out[4]);

/// FR-10 for one pixel (shade.cpp): `attr` as interpolated; the result 0..1 RGBA.
void shade(const platform::Pass3D& pass, const LightTables& tables, const platform::Material3DGpu& material,
           bool has_colors, const TextureView& texture, const float* attr, float out[4]);

/// A 0..1 channel → 0..255 (round half up, clamped).
std::uint8_t to_byte(float c);

} // namespace nnh::raster3d
