// Fake platform: 3D (contract nnh/platform/video3d.hpp). Every call is recorded (pass3d_log()); meshes keep a copy of
// their bytes (mesh_data()) so the pixel tests can rasterise them.

#include <cstring>
#include <new>

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
};

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
}
void detail::video3d_frame_begun() { g_3d.call_count = 0; }
void detail::video3d_frame_ended() {
    g_3d.last_frame_call_count = g_3d.call_count;
    std::memcpy(g_3d.last_frame_calls, g_3d.calls, sizeof g_3d.calls);
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
    --g_3d.live_meshes;
    g_3d.live_mesh_bytes -= m->size;
    delete[] m->bytes;
    delete m;
}

void begin_3d(Screen screen, const Pass3D& pass) {
    log_char('[');
    if (platform_fake::Fake3DCall* c = next_call('b')) c->screen = screen, c->pass = pass;
}

void draw_3d(const Draw3D& draw) {
    if (platform_fake::Fake3DCall* c = next_call('d')) c->draw = draw;
}

void end_3d() {
    log_char(']');
    next_call('e');
}

} // namespace platform
} // namespace nnh
