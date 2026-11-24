// 3D init and shutdown (gfx3d_state.hpp): the frame's records and hot arrays (one heap block, tag gfx3d), the mesh
// table and the platform's 3D, all reserved once (FR-26). Nothing is reserved with 3D off.

#include "gfx3d/gfx3d_state.hpp"
#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/video3d.hpp"

namespace nnh {

NNH_MEM_TAG(gfx3d);

namespace detail {
namespace {

State3D g_3d;
void* g_block = nullptr;

constexpr std::size_t kPerDraw = sizeof(DrawRecord) + sizeof(Sphere) + 3 * sizeof(std::uint16_t) + sizeof(float) + 2 * sizeof(bool);

} // namespace

State3D& state3d() { return g_3d; }

bool gfx3d_init(const GfxConfig& config) {
    const int max_draws = config.max_draws_3d > 4096 ? 4096 : config.max_draws_3d;
    g_3d = State3D{};
    g_block = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(gfx3d), kPerDraw * static_cast<std::size_t>(max_draws), 8);
    if (g_block == nullptr || !mesh_table_init()) {
        NNH_LOG_ERROR(gfx, "not enough memory for %d 3D draws (AppConfig::gfx.max_draws_3d)", max_draws);
        mem::free(g_block);
        g_block = nullptr;
        return false;
    }
    if (!platform::video3d_init({.max_draws = max_draws})) {
        NNH_LOG_ERROR(gfx, "3D could not start: %s", platform::last_error());
        mesh_table_shutdown();
        mem::free(g_block);
        g_block = nullptr;
        return false;
    }
    auto* p = static_cast<unsigned char*>(g_block);
    g_3d.records = reinterpret_cast<DrawRecord*>(p);
    p += sizeof(DrawRecord) * static_cast<std::size_t>(max_draws);
    g_3d.spheres = reinterpret_cast<Sphere*>(p);
    p += sizeof(Sphere) * static_cast<std::size_t>(max_draws);
    g_3d.keys = reinterpret_cast<float*>(p);
    p += sizeof(float) * static_cast<std::size_t>(max_draws);
    g_3d.order = reinterpret_cast<std::uint16_t*>(p);
    g_3d.scratch = g_3d.order + 2 * max_draws; // order: the opaque pass, then the blended one
    g_3d.visible = reinterpret_cast<bool*>(g_3d.scratch + max_draws);
    g_3d.blended = g_3d.visible + max_draws;
    g_3d.max_draws = max_draws;
    g_3d.last.max_draws = max_draws;
    g_3d.ready = true;
    return true;
}

void gfx3d_shutdown() {
    if (!g_3d.ready) return;
    mesh_table_shutdown();
    platform::video3d_shutdown();
    mem::free(g_block);
    g_block = nullptr;
    g_3d = State3D{};
}

bool gfx3d_ready() { return g_3d.ready; }

void gfx3d_frame_done() {
    if (!g_3d.ready) return;
    Stats3D& s = g_3d.last;
    s.draws = g_3d.count;
    s.culled = g_3d.frame_culled;
    s.triangles[0] = g_3d.frame_triangles[0], s.triangles[1] = g_3d.frame_triangles[1];
    s.eyes = g_3d.frame_eyes;
    if (g_3d.count > g_3d.peak) g_3d.peak = g_3d.count;
    s.peak_draws = g_3d.peak;
    if (g_3d.dropped > 0) {
        g_3d.dropped_total += g_3d.dropped;
        NNH_LOG_WARN(gfx, "%d 3D draws dropped this frame (AppConfig::gfx.max_draws_3d = %d)", g_3d.dropped,
                     g_3d.max_draws);
    }
    s.dropped_total = g_3d.dropped_total;
    s.meshes = mesh_count();
    s.mesh_bytes = mesh_bytes();
    g_3d.count = g_3d.dropped = g_3d.frame_culled = g_3d.frame_eyes = 0;
    g_3d.frame_triangles[0] = g_3d.frame_triangles[1] = 0;
}

} // namespace detail
} // namespace nnh
