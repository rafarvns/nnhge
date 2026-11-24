// glTF primitives (gltf_model.hpp): triangles only, attributes read and checked, positions and normals baked with
// their node's matrix (normals by the cofactor matrix, mirrored nodes' winding flipped), merged into one submesh per
// material in first-appearance order. Primitives without normals get flat ones (all of them: the engine's builder
// does it; some of them: unwelded here with the builder's face_normal, so the bytes match).

#include <cmath>

#include "gltf_model.hpp"
#include "mesh/mesh_quantize.hpp"

namespace nnh::assetc::gltf {
namespace {

struct Prim {
    const Placed* placed = nullptr;
    int material = -1;
    AccessorData pos, nrm, uv, col, idx;
    bool has_nrm = false, has_uv = false, has_col = false;
};

bool attribute(Context& ctx, const Model& m, const Prim& p, const char* name, AccessorData& out, int comps_a,
               int comps_b, bool ints_ok) {
    int index = 0;
    const json::Ref attrs = p.placed->primitive.get("attributes");
    if (!get_index(attrs, name, m.doc.root().get("accessors").size(), index))
        return ctx.fail(m.file, std::string("attribute ") + name + " names no accessor (at " + p.placed->where + ")");
    if (!read_accessor(ctx, m, index, p.placed->where + ".attributes." + name, out)) return false;
    const bool comps_ok = out.components == comps_a || out.components == comps_b;
    const bool type_ok = out.component_type == 5126 || (ints_ok && out.normalized && (out.component_type == 5121 || out.component_type == 5123));
    if (!comps_ok || !type_ok)
        return ctx.fail(m.file, std::string("attribute ") + name + " has an unsupported type (at " + p.placed->where + ")");
    if (out.count != p.pos.count && &out != &p.pos)
        return ctx.fail(m.file, std::string("attribute ") + name + " has a different count (at " + p.placed->where + ")");
    return true;
}

bool read_prim(Context& ctx, Model& m, const Placed& placed, int materials, Prim& p) {
    p.placed = &placed;
    const json::Ref pr = placed.primitive;
    if (pr.get("mode") && pr.get("mode").number() != 4)
        return ctx.fail(m.file, "only triangle primitives (mode 4) are supported (at " + placed.where + ")");
    const json::Ref attrs = pr.get("attributes");
    if (!attrs.get("POSITION")) return ctx.fail(m.file, "a primitive without POSITION (at " + placed.where + ")");
    if (!attribute(ctx, m, p, "POSITION", p.pos, 3, 3, false)) return false;
    p.has_nrm = static_cast<bool>(attrs.get("NORMAL"));
    p.has_uv = static_cast<bool>(attrs.get("TEXCOORD_0"));
    p.has_col = static_cast<bool>(attrs.get("COLOR_0"));
    if (p.has_nrm && !attribute(ctx, m, p, "NORMAL", p.nrm, 3, 3, false)) return false;
    if (p.has_uv && !attribute(ctx, m, p, "TEXCOORD_0", p.uv, 2, 2, true)) return false;
    if (p.has_col && !attribute(ctx, m, p, "COLOR_0", p.col, 3, 4, true)) return false;
    if (attrs.get("TEXCOORD_1")) m.notes.push_back("ignored: TEXCOORD_1 and later (" + placed.where + ")");
    if (pr.get("targets")) m.notes.push_back("ignored: morph targets (" + placed.where + ")");
    if (pr.get("material") && !get_index(pr, "material", materials, p.material))
        return ctx.fail(m.file, "a primitive names a material that doesn't exist (at " + placed.where + ")");
    if (pr.get("indices")) {
        int index = 0;
        if (!get_index(pr, "indices", m.doc.root().get("accessors").size(), index) ||
            !read_accessor(ctx, m, index, placed.where + ".indices", p.idx))
            return ctx.problem.reason.empty() ? ctx.fail(m.file, "bad indices (at " + placed.where + ")") : false;
        const int t = p.idx.component_type;
        if (p.idx.components != 1 || !(t == 5121 || t == 5123 || t == 5125) || p.idx.count % 3 != 0)
            return ctx.fail(m.file, "indices must be unsigned scalars in whole triangles (at " + placed.where + ")");
        for (const double v : p.idx.values)
            if (v >= p.pos.count) return ctx.fail(m.file, "an index is out of range (at " + placed.where + ")");
    } else {
        if (p.pos.count % 3 != 0) return ctx.fail(m.file, "a primitive without indices needs whole triangles (at " + placed.where + ")");
        p.idx.count = p.pos.count;
        p.idx.components = 1;
        p.idx.values.resize(static_cast<std::size_t>(p.pos.count));
        for (int i = 0; i < p.pos.count; ++i) p.idx.values[static_cast<std::size_t>(i)] = i;
    }
    return true;
}

struct Baker {
    DMat4 w;
    double c[9]; // cofactor matrix × sign(det): transforms normals
    bool mirrored = false;

    explicit Baker(const DMat4& world) : w(world) {
        const double* m = w.m;
        auto a = [m](int r, int col) { return m[col * 4 + r]; };
        c[0] = a(1, 1) * a(2, 2) - a(2, 1) * a(1, 2), c[1] = a(2, 1) * a(0, 2) - a(0, 1) * a(2, 2);
        c[2] = a(0, 1) * a(1, 2) - a(1, 1) * a(0, 2);
        c[3] = a(2, 0) * a(1, 2) - a(1, 0) * a(2, 2), c[4] = a(0, 0) * a(2, 2) - a(2, 0) * a(0, 2);
        c[5] = a(1, 0) * a(0, 2) - a(0, 0) * a(1, 2);
        c[6] = a(1, 0) * a(2, 1) - a(2, 0) * a(1, 1), c[7] = a(2, 0) * a(0, 1) - a(0, 0) * a(2, 1);
        c[8] = a(0, 0) * a(1, 1) - a(1, 0) * a(0, 1);
        const double det = a(0, 0) * c[0] + a(1, 0) * c[1] + a(2, 0) * c[2];
        mirrored = det < 0;
    }
    nnh::Vec3 point(const double* p) const {
        const double* m = w.m;
        return {float(m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]),
                float(m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]),
                float(m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14])};
    }
    nnh::Vec3 normal(const double* n) const { // c is the adjugate's rows: inverse = c / det, normals use its transpose
        const double s = mirrored ? -1.0 : 1.0;
        const double x = s * (c[0] * n[0] + c[3] * n[1] + c[6] * n[2]);
        const double y = s * (c[1] * n[0] + c[4] * n[1] + c[7] * n[2]);
        const double z = s * (c[2] * n[0] + c[5] * n[1] + c[8] * n[2]);
        const double l = std::sqrt(x * x + y * y + z * z);
        return l > 1e-30 ? nnh::Vec3{float(x / l), float(y / l), float(z / l)} : nnh::Vec3{0, 1, 0};
    }
};

std::uint8_t to_byte(double v) { return static_cast<std::uint8_t>(std::floor((v < 0 ? 0 : v > 1 ? 1 : v) * 255.0 + 0.5)); }

void push_vertex(const Prim& p, const Baker& b, int v, Arrays& out, bool flat_here, nnh::Vec3 flat_n) {
    out.positions.push_back(b.point(&p.pos.values[static_cast<std::size_t>(v) * 3]));
    if (out.has_normals) out.normals.push_back(flat_here ? flat_n : b.normal(&p.nrm.values[static_cast<std::size_t>(v) * 3]));
    if (out.has_uvs)
        out.uvs.push_back(p.has_uv ? nnh::Vec2{float(p.uv.values[static_cast<std::size_t>(v) * 2]),
                                               float(p.uv.values[static_cast<std::size_t>(v) * 2 + 1])}
                                   : nnh::Vec2{});
    if (out.has_colors) {
        const double* c = p.has_col ? &p.col.values[static_cast<std::size_t>(v) * p.col.components] : nullptr;
        out.colors.push_back(c == nullptr ? nnh::colors::white
                                          : nnh::Color{to_byte(c[0]), to_byte(c[1]), to_byte(c[2]),
                                                       p.col.components == 4 ? to_byte(c[3]) : std::uint8_t{255}});
    }
}

bool append(Context& ctx, const Model& m, const Prim& p, Arrays& out) {
    const Baker b(p.placed->world);
    const int base = static_cast<int>(out.positions.size());
    const bool unweld = out.has_normals && !p.has_nrm; // some primitives have normals, this one not
    const int adds = unweld ? p.idx.count : p.pos.count;
    if (base + adds > nnh::kMaxMeshVertices)
        return ctx.fail(m.file, "more than 65535 vertices after merging (split the model or weld vertices)");
    for (int t = 0; t < p.idx.count; t += 3) {
        int i[3] = {int(p.idx.values[t]), int(p.idx.values[t + 1]), int(p.idx.values[t + 2])};
        if (b.mirrored) std::swap(i[1], i[2]);
        if (unweld) {
            const nnh::Vec3 n = detail::mesh::face_normal(b.point(&p.pos.values[i[0] * 3]), b.point(&p.pos.values[i[1] * 3]),
                                                          b.point(&p.pos.values[i[2] * 3]));
            for (int k = 0; k < 3; ++k) {
                push_vertex(p, b, i[k], out, true, n);
                out.indices.push_back(static_cast<std::uint16_t>(out.positions.size() - 1));
            }
        } else {
            for (int k = 0; k < 3; ++k) out.indices.push_back(static_cast<std::uint16_t>(base + i[k]));
        }
    }
    if (!unweld)
        for (int v = 0; v < p.pos.count; ++v) push_vertex(p, b, v, out, false, {});
    return true;
}

} // namespace

bool merge_primitives(Context& ctx, Model& model, const std::vector<Placed>& placed,
                      const std::vector<MaterialInfo>& materials, Arrays& out) {
    std::vector<Prim> prims(placed.size());
    std::vector<int> order; // material ids in first-appearance order (−1 = default)
    bool all_normals = true, any_normals = false;
    for (std::size_t i = 0; i < placed.size(); ++i) {
        if (!read_prim(ctx, model, placed[i], static_cast<int>(materials.size()), prims[i])) return false;
        all_normals = all_normals && prims[i].has_nrm;
        any_normals = any_normals || prims[i].has_nrm;
        out.has_uvs = out.has_uvs || prims[i].has_uv;
        out.has_colors = out.has_colors || prims[i].has_col;
        bool seen = false;
        for (const int o : order) seen = seen || o == prims[i].material;
        if (!seen) order.push_back(prims[i].material);
    }
    out.has_normals = any_normals;
    if (order.size() > static_cast<std::size_t>(nnh::kMaxSubmeshes))
        return ctx.fail(model.file, "more than 32 materials are used (one submesh each)");
    for (const int mat : order) {
        nnh::SubmeshDesc sub;
        sub.first_index = static_cast<int>(out.indices.size());
        for (const Prim& p : prims)
            if (p.material == mat && !append(ctx, model, p, out)) return false;
        sub.index_count = static_cast<int>(out.indices.size()) - sub.first_index;
        if (mat >= 0) sub.material = materials[static_cast<std::size_t>(mat)].material;
        const int tex = mat >= 0 ? materials[static_cast<std::size_t>(mat)].texture : -1;
        out.submeshes.push_back(sub);
        out.submesh_textures.push_back(tex < 0 ? std::uint8_t{0xFF} : static_cast<std::uint8_t>(tex));
    }
    (void)all_normals;
    if (out.indices.size() > 786432u) return ctx.fail(model.file, "more than 262144 triangles");
    return true;
}

} // namespace nnh::assetc::gltf
