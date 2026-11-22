#include "action_log.hpp"

#include <cstdio>

void format_edge(char* out, std::size_t size, const char* action, bool pressed) {
    std::snprintf(out, size, "actions: %s %s", action, pressed ? "pressed" : "released");
}

void format_remap(char* out, std::size_t size, const nnh::RemapResult& r, const nnh::ActionSet& set) {
    const char* name = set.name(nnh::ActionId(r.action));
    char source[48];
    nnh::actions::describe(r.source, source, sizeof source);
    const char* other = set.name(nnh::ActionId(r.other_action));
    switch (r.state) {
    case nnh::ListenState::Bound:
        if (r.resolution == nnh::Resolution::Swapped) {
            std::snprintf(out, size, "actions: rebind %s -> %s (swapped with %s)", name, source, other);
        } else if (r.resolution == nnh::Resolution::Replaced) {
            std::snprintf(out, size, "actions: rebind %s -> %s (replaced %s)", name, source, other);
        } else {
            std::snprintf(out, size, "actions: rebind %s -> %s", name, source);
        }
        return;
    case nnh::ListenState::Rejected:
        std::snprintf(out, size, "actions: rebind %s rejected (%s is used by %s)", name, source, other);
        return;
    case nnh::ListenState::Cancelled: std::snprintf(out, size, "actions: rebind %s cancelled", name); return;
    case nnh::ListenState::TimedOut: std::snprintf(out, size, "actions: rebind %s timed out", name); return;
    case nnh::ListenState::Idle:
    case nnh::ListenState::Listening: break;
    }
    std::snprintf(out, size, "actions: rebind %s waiting", name);
}

const char* aim_line(bool has_c_stick) {
    return has_c_stick ? "actions: aim uses C-Stick" : "actions: aim uses L/R (no C-Stick)";
}

std::uint32_t bindings_sum(const void* data, std::size_t size) {
    const auto* p = static_cast<const unsigned char*>(data);
    std::uint32_t h = 0x811C9DC5u;
    for (std::size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}
