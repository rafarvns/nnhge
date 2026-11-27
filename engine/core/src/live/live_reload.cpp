// Hot reload meets live values (spec 031 FR-23, plan question 11; Debug/Development): a reload never writes a tunable.
// In the frame a batch is applied, the tunables whose value differs from their default (edited in the panel or by `nnh`)
// are noted after the application point (up to 16), and at that frame's end one whose value changed meanwhile — the
// game reloaded its data into the tuned field — logs one warning with both values. changed() is untouched. Linked only
// with the live module (LiveOps). Tests: tests/live/test_live_reload.cpp.

#include "live/live_apply.hpp"
#include "live/live_block.hpp"
#include "live/live_format.hpp"
#include "nnh/log.hpp"

namespace nnh {

NNH_LOG_CHANNEL_EXTERN(live);

namespace detail::live {
namespace {

struct Noted {
    int slot = 0;
    std::uint16_t generation = 0;
    std::uint32_t bits = 0;
};
constexpr int kMaxNoted = 16;
Noted g_noted[kMaxNoted];
int g_count = 0;

} // namespace

void reload_snapshot() {
    g_count = 0;
    const Registry* r = g_registry;
    if (r == nullptr) return;
    for (int i = 0; i < r->tunable_count && g_count < kMaxNoted; ++i) {
        const int slot = r->tunable_index[i];
        const Entry& e = r->entries[slot];
        if (e.kind == Kind::Action || e.target == nullptr) continue;
        const std::uint32_t bits = read_bits(e);
        if (bits != e.def) g_noted[g_count++] = Noted{slot, e.generation, bits};
    }
}

void reload_compare() {
    const Registry* r = g_registry;
    for (int i = 0; r != nullptr && i < g_count; ++i) {
        const Entry& e = r->entries[g_noted[i].slot];
        if (e.generation != g_noted[i].generation || e.kind == Kind::None) continue; // removed meanwhile
        const std::uint32_t now = read_bits(e);
        if (now == g_noted[i].bits) continue;
        char panel[kValueTextBytes], file[kValueTextBytes];
        format_bits(e.kind, g_noted[i].bits, false, panel);
        format_bits(e.kind, now, false, file);
        NNH_LOG_WARN(live, "live: %.*s was set by a reload (panel value %s, file value %s)", e.name_length, e.name, panel,
                     file);
    }
    g_count = 0;
}

} // namespace detail::live
} // namespace nnh
