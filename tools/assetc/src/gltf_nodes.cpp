// The default scene's nodes (gltf_model.hpp): each node's matrix (or translation × rotation × scale) composed with
// its parents' in double, depth ≤ 32, every node reached once; each mesh primitive collected with its world matrix.

#include <cmath>

#include "gltf_model.hpp"

namespace nnh::assetc::gltf {
namespace {

json::Ref at(json::Ref array, int index) {
    json::Ref r = array.first();
    for (int i = 0; i < index && r; ++i) r = r.next();
    return r;
}

bool numbers(json::Ref array, int n, double* out) {
    if (!array.is_array() || array.size() != n) return false;
    int i = 0;
    for (json::Ref v = array.first(); v; v = v.next(), ++i) {
        if (!v.is_number()) return false;
        out[i] = v.number();
    }
    return true;
}

/// The node's local matrix. False when malformed.
bool local_matrix(json::Ref node, DMat4& m) {
    if (node.get("matrix")) return numbers(node.get("matrix"), 16, m.m);
    double t[3] = {0, 0, 0}, r[4] = {0, 0, 0, 1}, s[3] = {1, 1, 1};
    if (node.get("translation") && !numbers(node.get("translation"), 3, t)) return false;
    if (node.get("rotation") && !numbers(node.get("rotation"), 4, r)) return false;
    if (node.get("scale") && !numbers(node.get("scale"), 3, s)) return false;
    const double n = r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3];
    if (!(n > 1e-12)) return false;
    const double x = r[0], y = r[1], z = r[2], w = r[3];
    const double rot[9] = {(w * w + x * x - y * y - z * z) / n, 2 * (x * y + w * z) / n, 2 * (x * z - w * y) / n,
                           2 * (x * y - w * z) / n, (w * w - x * x + y * y - z * z) / n, 2 * (y * z + w * x) / n,
                           2 * (x * z + w * y) / n, 2 * (y * z - w * x) / n, (w * w - x * x - y * y + z * z) / n};
    for (int c = 0; c < 3; ++c)
        for (int row = 0; row < 3; ++row) m.m[c * 4 + row] = rot[c * 3 + row] * s[c];
    m.m[12] = t[0], m.m[13] = t[1], m.m[14] = t[2];
    return true;
}

struct Walk {
    Context& ctx;
    const Model& model;
    std::vector<int> visits;
    std::vector<Placed>& out;
    int primitives = 0;

    bool node(int index, const DMat4& parent, int depth) {
        const std::string where = "$.nodes[" + std::to_string(index) + "]";
        if (depth > kMaxNodeDepth) return ctx.fail(model.file, "nodes nest deeper than 32 (at " + where + ")");
        if (visits[static_cast<std::size_t>(index)]++ > 0)
            return ctx.fail(model.file, "a node is reached twice (a cycle or a shared node) (at " + where + ")");
        const json::Ref n = at(model.doc.root().get("nodes"), index);
        DMat4 local;
        if (!n.is_object() || !local_matrix(n, local))
            return ctx.fail(model.file, "a node's matrix, translation, rotation or scale is malformed (at " + where + ")");
        const DMat4 world = mul(parent, local);
        int mesh = 0;
        if (n.get("mesh") && !get_index(n, "mesh", model.doc.root().get("meshes").size(), mesh))
            return ctx.fail(model.file, "a node names a mesh that doesn't exist (at " + where + ")");
        if (n.get("mesh")) {
            int p = 0;
            const json::Ref prims = at(model.doc.root().get("meshes"), mesh).get("primitives");
            if (!prims.is_array() || prims.size() == 0)
                return ctx.fail(model.file, "a mesh without primitives (at $.meshes[" + std::to_string(mesh) + "])");
            for (json::Ref pr = prims.first(); pr; pr = pr.next(), ++p) {
                if (++primitives > kMaxPrimitives) return ctx.fail(model.file, "more than 256 primitives in the scene");
                out.push_back({pr, "$.meshes[" + std::to_string(mesh) + "].primitives[" + std::to_string(p) + "]", world});
            }
        }
        const int count = model.doc.root().get("nodes").size();
        for (json::Ref c = n.get("children").first(); c; c = c.next()) {
            const double d = c.number();
            if (!c.is_number() || d < 0 || d >= count || d != std::floor(d))
                return ctx.fail(model.file, "a child index is out of range (at " + where + ".children)");
            if (!node(static_cast<int>(d), world, depth + 1)) return false;
        }
        return true;
    }
};

} // namespace

bool collect_primitives(Context& ctx, Model& model, std::vector<Placed>& out) {
    const json::Ref root = model.doc.root();
    const int scenes = root.get("scenes").size();
    int scene = 0;
    if (scenes == 0) return ctx.fail(model.file, "the model has no scene: export with a scene");
    if (root.get("scene") && !get_index(root, "scene", scenes, scene))
        return ctx.fail(model.file, "'scene' names a scene that doesn't exist");
    Walk walk{ctx, model, std::vector<int>(static_cast<std::size_t>(root.get("nodes").size()), 0), out};
    const json::Ref s = at(root.get("scenes"), scene);
    for (json::Ref n = s.get("nodes").first(); n; n = n.next()) {
        const double d = n.number();
        if (!n.is_number() || d < 0 || d >= root.get("nodes").size() || d != std::floor(d))
            return ctx.fail(model.file, "a scene node index is out of range (at $.scenes[" + std::to_string(scene) + "])");
        if (!walk.node(static_cast<int>(d), DMat4{}, 1)) return false;
    }
    if (out.empty()) return ctx.fail(model.file, "the default scene has no mesh");
    return true;
}

} // namespace nnh::assetc::gltf
