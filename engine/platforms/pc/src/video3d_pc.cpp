// PC simulator: 3D (contract nnh/platform/video3d.hpp, decision D41). The engine's CPU rasteriser draws each pass into
// a 400×240 colour and depth buffer: the colour starts as the top screen's clear colour, or as the top target read back
// when 2D was drawn below the world (plan P15); the finished picture goes back onto the target opaque, 1:1, so every
// blend happens on the CPU and the PNGs are the same with every renderer. Meshes are copied to the heap.

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "context_pc.hpp"
#include "nnh/platform/video3d.hpp"
#include "video3d_pc.hpp"

namespace nnh::platform {
namespace {

constexpr int kW = 400, kH = 240;

struct PcMesh {
    raster3d::MeshView view{};
    std::size_t bytes = 0;
};

struct State {
    bool ready = false;
    SDL_Texture* picture = nullptr; // streaming RGBA32, drawn onto the top target at end_3d
    std::vector<std::uint8_t> rgba;
    std::vector<float> depth;
    raster3d::LightTables tables;
    Pass3D pass{};
    Screen screen = Screen::Top;
};
State* g = nullptr;

raster3d::Target target() { return {g->rgba.data(), g->depth.data(), kW, kH}; }

/// The top target's pixels (2D drawn below the world) into the colour buffer.
bool read_top() {
    SDL_Surface* shot = SDL_RenderReadPixels(pc::context().renderer, nullptr);
    if (shot == nullptr) return false;
    SDL_Surface* rgba = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(shot);
    if (rgba == nullptr || rgba->w != kW || rgba->h != kH) {
        SDL_DestroySurface(rgba);
        return false;
    }
    for (int y = 0; y < kH; ++y)
        std::memcpy(g->rgba.data() + static_cast<std::size_t>(y) * kW * 4,
                    static_cast<const std::uint8_t*>(rgba->pixels) + static_cast<std::size_t>(y) * rgba->pitch, kW * 4);
    SDL_DestroySurface(rgba);
    return true;
}

} // namespace

bool video3d_init(const Video3DConfig&) {
    g = new (std::nothrow) State;
    if (g == nullptr) return false;
    g->rgba.resize(static_cast<std::size_t>(kW) * kH * 4);
    g->depth.resize(static_cast<std::size_t>(kW) * kH);
    raster3d::make_light_tables(g->tables);
    g->picture = SDL_CreateTexture(pc::context().renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, kW, kH);
    if (g->picture == nullptr) {
        pc::set_error("3D picture texture");
        delete g;
        g = nullptr;
        return false;
    }
    SDL_SetTextureBlendMode(g->picture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(g->picture, SDL_SCALEMODE_NEAREST);
    g->ready = true;
    return true;
}

void video3d_shutdown() {
    if (g == nullptr) return;
    SDL_DestroyTexture(g->picture);
    delete g;
    g = nullptr;
}

const char* video3d_self_test() { return "3d tilt: PASS (simulator: no rotated screen to match)"; }

bool mesh_create(const MeshUpload& up, MeshInfo& out) {
    out = {};
    const std::size_t vbytes = static_cast<std::size_t>(up.vertex_count) * up.stride, ibytes = up.index_count * 2u;
    auto* m = static_cast<PcMesh*>(std::malloc(sizeof(PcMesh) + vbytes + ibytes));
    if (m == nullptr) {
        pc::set_error("no memory for a mesh");
        return false;
    }
    auto* bytes = reinterpret_cast<std::uint8_t*>(m + 1);
    std::memcpy(bytes, up.vertices, vbytes);
    std::memcpy(bytes + vbytes, up.indices, ibytes);
    m->view = {bytes, up.vertex_count, up.stride, up.flags, reinterpret_cast<const std::uint16_t*>(bytes + vbytes),
               up.index_count};
    m->bytes = vbytes + ibytes;
    out.handle = reinterpret_cast<MeshHandle>(m);
    out.bytes = m->bytes; // what the console's linear memory holds
    return true;
}

void mesh_destroy(MeshHandle handle) { std::free(reinterpret_cast<PcMesh*>(handle)); }

void begin_3d(Screen screen, const Pass3D& pass) {
    if (g == nullptr) return;
    g->pass = pass;
    g->screen = screen;
    pc::draw_on(screen);
    if (!pass.below_2d || !read_top()) {
        raster3d::clear(target(), pass.clear);
    } else {
        raster3d::clear_depth(target());
    }
}

void draw_3d(const Draw3D& d) {
    if (g == nullptr || d.mesh == 0) return;
    const raster3d::TextureView tex = d.material.texture != 0 ? pc::texels3d(d.material.texture) : raster3d::TextureView{};
    raster3d::draw(target(), g->pass, g->tables, d, reinterpret_cast<const PcMesh*>(d.mesh)->view, tex);
}

void end_3d() {
    if (g == nullptr) return;
    SDL_Renderer* r = pc::context().renderer;
    SDL_UpdateTexture(g->picture, nullptr, g->rgba.data(), kW * 4);
    pc::draw_on(g->screen);
    const SDL_FRect dst{0, 0, static_cast<float>(kW), static_cast<float>(kH)};
    SDL_RenderTexture(r, g->picture, nullptr, &dst);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}

} // namespace nnh::platform
