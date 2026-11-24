// The Mesh handle (nnh/mesh.hpp): move-only, RAII, queries through the mesh table; texture_id().

#include "gfx/gfx_state.hpp"
#include "mesh/mesh_internal.hpp"

namespace nnh {

TextureId texture_id(const Texture& texture) {
    if (!texture.valid()) return {};
    return {TextureAccess::slot(texture), TextureAccess::generation(texture)};
}

Mesh::Mesh(Mesh&& other) noexcept : slot_(other.slot_), generation_(other.generation_) {
    other.slot_ = -1;
    other.generation_ = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        reset();
        slot_ = other.slot_;
        generation_ = other.generation_;
        other.slot_ = -1;
        other.generation_ = 0;
    }
    return *this;
}

Mesh::~Mesh() { reset(); }

void Mesh::reset() {
    if (slot_ >= 0) detail::mesh_release(slot_, generation_);
    slot_ = -1;
    generation_ = 0;
}

bool Mesh::valid() const { return detail::mesh_get(slot_, generation_) != nullptr; }

int Mesh::submesh_count() const {
    const detail::MeshRecord* r = detail::mesh_get(slot_, generation_);
    return r != nullptr ? r->submesh_count : 0;
}

int Mesh::vertex_count() const {
    const detail::MeshRecord* r = detail::mesh_get(slot_, generation_);
    return r != nullptr ? r->vertex_count : 0;
}

int Mesh::triangle_count() const {
    const detail::MeshRecord* r = detail::mesh_get(slot_, generation_);
    return r != nullptr ? r->index_count / 3 : 0;
}

Aabb3 Mesh::bounds() const {
    const detail::MeshRecord* r = detail::mesh_get(slot_, generation_);
    return r != nullptr ? r->bounds : Aabb3{};
}

Sphere Mesh::bounding_sphere() const {
    const detail::MeshRecord* r = detail::mesh_get(slot_, generation_);
    return r != nullptr ? r->sphere : Sphere{};
}

Material3D Mesh::material(int submesh) const {
    const detail::MeshRecord* r = detail::mesh_get(slot_, generation_);
    if (r == nullptr || submesh < 0 || submesh >= r->submesh_count) return {};
    return r->submeshes[submesh].material;
}

} // namespace nnh
