// Built-in meshes (nnh/mesh.hpp, FR-5): cube, plane, UV sphere, built as arrays (load time, heap) and handed to
// create_mesh. Angles through sin_cos: the same bytes everywhere. -ffp-contract=off.

#include "memory/memory_internal.hpp"
#include "mesh/mesh_internal.hpp"

namespace nnh {
namespace {

/// The arrays of one primitive, in one heap block.
struct Arrays {
    void* block = nullptr;
    Vec3* positions = nullptr;
    Vec3* normals = nullptr;
    Vec2* uvs = nullptr;
    std::uint16_t* indices = nullptr;
    int vertex_count = 0;
    int index_count = 0;

    bool allocate(int vertices, int indices_n) {
        const std::size_t bytes = sizeof(Vec3) * 2 * vertices + sizeof(Vec2) * vertices + 2 * indices_n;
        block = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(meshes), bytes, alignof(Vec3));
        if (block == nullptr) return false;
        positions = static_cast<Vec3*>(block);
        normals = positions + vertices;
        uvs = reinterpret_cast<Vec2*>(normals + vertices);
        indices = reinterpret_cast<std::uint16_t*>(uvs + vertices);
        return true;
    }
    ~Arrays() { mem::free(block); }

    Result<Mesh> make(const Material3D& material) const {
        const SubmeshDesc sub{0, index_count, material};
        return create_mesh({.positions = positions, .vertex_count = vertex_count, .normals = normals, .uvs = uvs,
                            .indices = indices, .index_count = index_count, .submeshes = &sub, .submesh_count = 1});
    }
    void vertex(Vec3 p, Vec3 n, Vec2 uv) {
        positions[vertex_count] = p, normals[vertex_count] = n, uvs[vertex_count] = uv;
        ++vertex_count;
    }
    void triangle(int a, int b, int c) {
        indices[index_count++] = static_cast<std::uint16_t>(a);
        indices[index_count++] = static_cast<std::uint16_t>(b);
        indices[index_count++] = static_cast<std::uint16_t>(c);
    }
};

Error bad(const char* what) { return Error::make(ErrorCode::InvalidArgument, "%s", what); }
bool positive(float f) { return f > 1e-6f && f < 1048576.0f; }

} // namespace

Result<Mesh> make_cube(float size, const Material3D& material) {
    if (!detail::mesh_table_ready()) return detail::mesh_3d_off("make_cube()");
    if (!positive(size)) return bad("make_cube(): the size must be positive");
    Arrays a;
    if (!a.allocate(24, 36)) return Error::make(ErrorCode::OutOfMemory, "make_cube(): no memory");
    const float h = size * 0.5f;
    // Per face: normal n, right u, up v with u × v = n.
    const Vec3 faces[6][3] = {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},  {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
                              {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},  {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
                              {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},   {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}};
    for (const auto& f : faces) {
        const Vec3 c = f[0] * h, u = f[1] * h, v = f[2] * h;
        const int base = a.vertex_count;
        a.vertex(c - u - v, f[0], {0, 1});
        a.vertex(c + u - v, f[0], {1, 1});
        a.vertex(c + u + v, f[0], {1, 0});
        a.vertex(c - u + v, f[0], {0, 0});
        a.triangle(base, base + 1, base + 2);
        a.triangle(base, base + 2, base + 3);
    }
    return a.make(material);
}

Result<Mesh> make_plane(float width, float depth, int subdivisions, const Material3D& material) {
    if (!detail::mesh_table_ready()) return detail::mesh_3d_off("make_plane()");
    if (!positive(width) || !positive(depth) || subdivisions < 1 || subdivisions > 64)
        return bad("make_plane(): positive sizes and 1..64 subdivisions");
    const int n = subdivisions;
    Arrays a;
    if (!a.allocate((n + 1) * (n + 1), n * n * 6)) return Error::make(ErrorCode::OutOfMemory, "make_plane(): no memory");
    for (int j = 0; j <= n; ++j)
        for (int i = 0; i <= n; ++i) {
            const float fi = static_cast<float>(i) / static_cast<float>(n), fj = static_cast<float>(j) / static_cast<float>(n);
            a.vertex({(fi - 0.5f) * width, 0, (fj - 0.5f) * depth}, {0, 1, 0}, {fi, fj});
        }
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            const int p = j * (n + 1) + i, q = p + n + 1; // q: the next row (+z)
            a.triangle(p, q, q + 1);
            a.triangle(p, q + 1, p + 1);
        }
    return a.make(material);
}

Result<Mesh> make_sphere(float radius, int rings, int segments, const Material3D& material) {
    if (!detail::mesh_table_ready()) return detail::mesh_3d_off("make_sphere()");
    if (!positive(radius) || rings < 2 || rings > 64 || segments < 3 || segments > 128)
        return bad("make_sphere(): a positive radius, 2..64 rings, 3..128 segments");
    Arrays a;
    if (!a.allocate((rings + 1) * (segments + 1), rings * segments * 6))
        return Error::make(ErrorCode::OutOfMemory, "make_sphere(): no memory");
    for (int r = 0; r <= rings; ++r) {
        const float fr = static_cast<float>(r) / static_cast<float>(rings);
        const SinCos t = sin_cos(kHalfTurn * fr); // from the north pole (+y) down
        for (int s = 0; s <= segments; ++s) {
            const float fs = static_cast<float>(s) / static_cast<float>(segments);
            const SinCos p = sin_cos(kFullTurn * fs);
            const Vec3 n{t.sin * p.sin, t.cos, t.sin * p.cos};
            a.vertex(n * radius, n, {fs, fr});
        }
    }
    for (int r = 0; r < rings; ++r)
        for (int s = 0; s < segments; ++s) {
            const int p = r * (segments + 1) + s, q = p + segments + 1; // q: the ring below
            if (r != 0) a.triangle(p, q, p + 1);                 // the poles' degenerate halves are skipped
            if (r != rings - 1) a.triangle(p + 1, q, q + 1);
        }
    return a.make(material);
}

} // namespace nnh
