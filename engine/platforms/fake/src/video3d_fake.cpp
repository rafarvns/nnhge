// Fake platform: 3D (contract nnh/platform/video3d.hpp). Every call is recorded (pass3d_log()); meshes keep a copy of
// their bytes (mesh_data()) so the pixel tests can rasterise them.

#include <cstring>
#include <map>
#include <new>
#include <vector>

#include "raster3d.hpp"

#include "fake_internal.hpp"
#include "nnh/platform/video3d.hpp"

namespace nnh {
namespace {

platform_fake::Fake3D g_3d;
bool g_init_fails = false;
bool g_mesh_fails = false;

struct FakeMesh {
    std::size_t size = 0;
    std::uint8_t* bytes = nullptr; // vertices then indices
    raster3d::MeshView view{};
};

// Pixels (enable_pixels_3d): the top screen's picture per eye, its depth, the textures' RGBA copies, the tables.
bool g_pixels = false;
std::vector<std::uint8_t> g_rgba[2] = {std::vector<std::uint8_t>(400 * 240 * 4), std::vector<std::uint8_t>(400 * 240 * 4)};
std::vector<float> g_depth[2] = {std::vector<float>(400 * 240), std::vector<float>(400 * 240)};
std::vector<std::uint8_t> g_last_rgba[2] = {std::vector<std::uint8_t>(400 * 240 * 4),
                                            std::vector<std::uint8_t>(400 * 240 * 4)}; // the frame that ended last
std::map<platform::TextureHandle, std::pair<int, std::vector<std::uint8_t>>> g_textures; // handle → (width, rgba)
raster3d::LightTables g_tables;
platform::Pass3D g_pass;
bool g_shown_stereo = false; // what the "screen" shows (3DS: gfxSet3D), kept across frames

raster3d::Target target() {
    const int e = g_3d.eye == 1 ? 1 : 0;
    return {g_rgba[e].data(), g_depth[e].data(), 400, 240};
}

void log_char(char c) {
    platform_fake::State& s = platform_fake::detail::mutable_state();
    const std::size_t n = std::strlen(s.calls);
    if (n + 1 < sizeof s.calls) {
        s.calls[n] = c;
        s.calls[n + 1] = '\0';
    }
}

platform_fake::Fake3DCall* next_call(char kind) {
    const int i = g_3d.call_count++;
    if (i >= platform_fake::kMaxRecorded3D) return nullptr;
    g_3d.calls[i] = {};
    g_3d.calls[i].kind = kind;
    g_3d.calls[i].eye = g_3d.eye;
    return &g_3d.calls[i];
}

} // namespace

namespace platform_fake {

const Fake3D& pass3d_log() { return g_3d; }
void set_video3d_init_fails(bool fails) { g_init_fails = fails; }
void set_mesh_create_fails(bool fails) { g_mesh_fails = fails; }

const std::uint8_t* mesh_data(platform::MeshHandle handle, std::size_t& size) {
    const auto* m = reinterpret_cast<const FakeMesh*>(handle);
    size = m != nullptr ? m->size : 0;
    return m != nullptr ? m->bytes : nullptr;
}

void detail::video3d_reset() {
    g_3d = {};
    g_init_fails = g_mesh_fails = false;
    g_pixels = false;
    g_shown_stereo = false;
    g_textures.clear();
}
void detail::video3d_frame_begun() {
    g_3d.call_count = 0;
    g_3d.stereo = false;
    g_3d.eye = 0;
    g_3d.right_clears = 0;
    if (g_pixels) raster3d::clear(target(), detail::mutable_state().top_clear);
}

void detail::texture_recorded(platform::TextureHandle handle, int width, int height, const void* rgba) {
    std::vector<std::uint8_t> px(static_cast<std::size_t>(width) * height * 4);
    if (rgba != nullptr) {
        std::memcpy(px.data(), rgba, px.size());
    } else {
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                std::uint8_t* p = px.data() + (static_cast<std::size_t>(y) * width + x) * 4;
                p[0] = static_cast<std::uint8_t>(width > 1 ? x * 255 / (width - 1) : 0);
                p[1] = static_cast<std::uint8_t>(height > 1 ? y * 255 / (height - 1) : 0);
                p[2] = 0, p[3] = 255;
            }
    }
    g_textures[handle] = {width, std::move(px)};
}

void detail::texture_forgotten(platform::TextureHandle handle) { g_textures.erase(handle); }

void enable_pixels_3d(bool on) {
    g_pixels = on;
    raster3d::make_light_tables(g_tables);
    raster3d::clear(target(), detail::mutable_state().top_clear);
}

Color pixel_3d(int x, int y) { return pixel_3d(0, x, y); }

Color pixel_3d(std::uint8_t eye, int x, int y) {
    if (x < 0 || y < 0 || x >= 400 || y >= 240) return {};
    const std::uint8_t* p = g_last_rgba[eye == 1 ? 1 : 0].data() + (static_cast<std::size_t>(y) * 400 + x) * 4;
    return {p[0], p[1], p[2], p[3]};
}
void detail::video3d_frame_ended() {
    g_3d.last_frame_call_count = g_3d.call_count;
    std::memcpy(g_3d.last_frame_calls, g_3d.calls, sizeof g_3d.calls);
    g_3d.last_frame_stereo = g_3d.stereo;
    g_3d.last_frame_right_clears = g_3d.right_clears;
    if (g_pixels) g_last_rgba[0] = g_rgba[0], g_last_rgba[1] = g_rgba[1];
}

} // namespace platform_fake

namespace platform {

bool video3d_init(const Video3DConfig& config) {
    ++g_3d.init_calls;
    if (g_init_fails) {
        platform_fake::detail::set_last_error("fake: no VRAM for 3D");
        return false;
    }
    g_3d.initialized = true;
    g_3d.max_draws = config.max_draws;
    return true;
}

void video3d_shutdown() {
    ++g_3d.shutdown_calls;
    g_3d.initialized = false;
}

const char* video3d_self_test() { return nullptr; }

bool mesh_create(const MeshUpload& upload, MeshInfo& out) {
    out = {};
    if (g_mesh_fails) {
        platform_fake::detail::set_last_error("fake: no linear memory for the mesh");
        return false;
    }
    const std::size_t vbytes = static_cast<std::size_t>(upload.vertex_count) * upload.stride;
    const std::size_t ibytes = static_cast<std::size_t>(upload.index_count) * 2;
    auto* m = new (std::nothrow) FakeMesh;
    auto* bytes = new (std::nothrow) std::uint8_t[vbytes + ibytes];
    if (m == nullptr || bytes == nullptr) {
        delete m;
        delete[] bytes;
        return false;
    }
    std::memcpy(bytes, upload.vertices, vbytes);
    std::memcpy(bytes + vbytes, upload.indices, ibytes);
    m->size = vbytes + ibytes;
    m->bytes = bytes;
    m->view = {bytes, upload.vertex_count, upload.stride, upload.flags,
               reinterpret_cast<const std::uint16_t*>(bytes + vbytes), upload.index_count};
    out.handle = reinterpret_cast<MeshHandle>(m);
    out.bytes = vbytes + ibytes;
    ++g_3d.meshes_created;
    ++g_3d.live_meshes;
    g_3d.live_mesh_bytes += out.bytes;
    return true;
}

void mesh_destroy(MeshHandle handle) {
    auto* m = reinterpret_cast<FakeMesh*>(handle);
    if (m == nullptr) return;
    if (platform_fake::detail::mutable_state().gpu_busy) ++platform_fake::detail::mutable_state().destroyed_while_gpu_busy;
    --g_3d.live_meshes;
    g_3d.live_mesh_bytes -= m->size;
    delete[] m->bytes;
    delete m;
}

void begin_3d(Screen screen, const Pass3D& pass) {
    log_char('[');
    if (platform_fake::Fake3DCall* c = next_call('b')) c->screen = screen, c->pass = pass;
    g_pass = pass;
    if (g_pixels) raster3d::clear_depth(target());
}

void draw_3d(const Draw3D& draw) {
    if (platform_fake::Fake3DCall* c = next_call('d')) c->draw = draw;
    if (!g_pixels || draw.mesh == 0) return;
    raster3d::TextureView tex;
    const auto t = g_textures.find(draw.material.texture);
    if (t != g_textures.end()) {
        tex.rgba = t->second.second.data();
        tex.width = t->second.first;
        tex.height = static_cast<int>(t->second.second.size() / 4 / static_cast<std::size_t>(t->second.first));
    }
    raster3d::draw(target(), g_pass, g_tables, draw, reinterpret_cast<const FakeMesh*>(draw.mesh)->view, tex);
}

void end_3d() {
    log_char(']');
    next_call('e');
}

void set_stereo(bool on) {
    ++g_3d.set_stereo_calls;
    g_3d.stereo = on;
    if (on != g_shown_stereo) ++g_3d.stereo_changes;
    g_shown_stereo = on;
}

void select_eye(std::uint8_t eye, Color top_clear) {
    g_3d.eye = eye == 1 ? 1 : 0;
    log_char(g_3d.eye == 1 ? 'R' : 'L');
    next_call('s');
    if (g_3d.eye == 1 && g_3d.right_clears == 0) {
        ++g_3d.right_clears;
        g_3d.right_clear = top_clear;
        if (g_pixels) raster3d::clear(target(), top_clear);
    }
}

} // namespace platform
} // namespace nnh
