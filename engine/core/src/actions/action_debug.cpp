// The debug view (spec 017 FR-24): two lines of debug text, the enabled contexts and the actions held this frame.
// Debug/Development only: in Release the body is compiled out, so no names or format strings reach the binary.

#include "actions/actions_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/debug.hpp"

namespace nnh {
namespace detail {
namespace {

[[maybe_unused]] void append(char* out, std::size_t cap, std::size_t& n, const char* text) {
    for (; *text != '\0' && n + 1 < cap; ++text) out[n++] = *text;
    out[n] = '\0';
}

} // namespace
} // namespace detail

void ActionSet::debug_draw(Screen screen, int x, int y) const {
    if constexpr (build::kDevToolsEnabled) {
        if (block_ == nullptr) return;
        char line[256];
        std::size_t n = 0;
        detail::append(line, sizeof line, n, "actions:");
        for (int c = 0; c < block_->contexts; ++c) {
            if ((block_->ctx[c].flags & detail::context_flag::kOn) == 0) continue;
            detail::append(line, sizeof line, n, " ");
            detail::append(line, sizeof line, n, block_->ctx[c].name);
        }
        debug::print(screen, x, y, line);
        n = 0;
        detail::append(line, sizeof line, n, "held:");
        const std::size_t empty = n;
        for (int i = 0; i < block_->declared; ++i) {
            const detail::ActionRec& r = block_->actions[block_->order[i]];
            if ((r.flags & detail::action_flag::kHeld) == 0) continue;
            detail::append(line, sizeof line, n, " ");
            detail::append(line, sizeof line, n, r.name);
        }
        if (n == empty) detail::append(line, sizeof line, n, " -");
        debug::print(screen, x, y + debug::line_height(), line);
    } else {
        (void)screen;
        (void)x;
        (void)y;
    }
}

} // namespace nnh
