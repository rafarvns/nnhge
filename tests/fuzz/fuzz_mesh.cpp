// .nnhmesh reader fuzz target (spec 026 FR-7): validated as given, then with the header's size and CRC fixed (so
// mutations reach the deep checks); an accepted file is walked like the loader and the backends walk it — every
// submesh's indices and vertices read, every texture path — and must stay in bounds.

#include <cstring>
#include <vector>

#include "core/crc32.hpp"
#include "fuzz_targets.hpp"
#include "mesh/mesh_validate.hpp"

namespace nnh::fuzz {
namespace {

std::uint32_t walk(const detail::mesh::MeshView& v) {
    std::uint32_t acc = 0;
    for (int s = 0; s < v.header.submesh_count; ++s) {
        const detail::mesh::SubmeshRecord r = detail::mesh::submesh(v, s);
        for (std::uint32_t i = r.first_index; i < r.first_index + r.index_count; ++i) {
            std::uint16_t index;
            std::memcpy(&index, v.indices + 2 * i, 2);
            const std::uint8_t* vertex = v.vertices + static_cast<std::size_t>(index) * v.header.stride;
            for (int b = 0; b < v.header.stride; ++b) acc += vertex[b];
        }
    }
    for (int t = 0; t < v.header.texture_count; ++t) acc += static_cast<std::uint32_t>(std::strlen(v.textures + 64 * t));
    return acc;
}

bool check(const std::uint8_t* data, std::size_t size) {
    detail::mesh::MeshView v;
    char why[64];
    if (!detail::mesh::validate(data, size, v, why, sizeof why)) return false;
    volatile std::uint32_t sink = walk(v);
    (void)sink;
    return true;
}

} // namespace

bool mesh_fixed_accepted(const std::uint8_t* data, std::size_t size) {
    if (size < sizeof(detail::mesh::MeshHeader) || size > (1u << 20)) return false;
    std::vector<std::uint8_t> fixed(data, data + size);
    const auto n = static_cast<std::uint32_t>(size);
    std::memcpy(fixed.data() + 12, &n, 4);
    const std::uint32_t crc = detail::crc32(fixed.data() + 112, size - 112);
    std::memcpy(fixed.data() + 16, &crc, 4);
    return check(fixed.data(), fixed.size());
}

int mesh_reader_target(const std::uint8_t* data, std::size_t size) {
    check(data, size);
    mesh_fixed_accepted(data, size);
    return 0;
}

} // namespace nnh::fuzz
