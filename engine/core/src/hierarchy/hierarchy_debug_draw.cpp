// The hierarchy's debug draw (spec 022 FR-22): for each node, a line to its parent and its local axes (8 px), from the
// stored world transforms. Debug and Development only: a Release build compiles the body away. The only hierarchy
// source that includes <nnh/gfx.hpp>.

#include "hierarchy/hierarchy_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/gfx.hpp"

namespace nnh {

void Hierarchy::debug_draw(Screen screen) const {
    if constexpr (!build::kDevToolsEnabled) {
        (void)screen;
    } else {
        const detail::Tree t = detail::tree_of(*this);
        if (!t.valid()) return;
        const detail::SlotTable& slots = t.w->kinds[t.kind].slots;
        constexpr Color kLink = rgba(255, 255, 0, 200), kAxisX = rgba(255, 64, 64, 255), kAxisY = rgba(64, 255, 64, 255);
        for (int s = 0; s < slots.extent; ++s) {
            if (!slots.is_live(s)) continue;
            const Node& n = t.nodes[s];
            const WorldTransform2D& w = n.world();
            const std::uint16_t p = detail::NA::parent(n);
            if (p != detail::kNoNode) gfx::draw_line(screen, w.position(), t.nodes[p].world().position(), kLink);
            gfx::draw_line(screen, w.position(), transform_point(w.map, {8, 0}), kAxisX);
            gfx::draw_line(screen, w.position(), transform_point(w.map, {0, 8}), kAxisY);
        }
    }
}

} // namespace nnh
