// Tunables and actions (spec 030 FR-1, FR-2, FR-4, FR-5, FR-6, Q9): the back ends of <nnh/live.hpp>'s inline calls.
// Every check asserts with the reason; a refused registration returns no handle (id 0).

#include <cmath>
#include <cstring>

#include "live/live_apply.hpp"
#include "live/live_names.hpp"
#include "live/live_registry.hpp"
#include "nnh/app.hpp"
#include "nnh/assert.hpp"
#include "nnh/gfx.hpp"
#include "nnh/live.hpp"

namespace nnh::detail::live_api {
namespace {

using namespace nnh::detail::live;

/// The registry and a usable name, or null.
Registry* start(const nnh::live::Name& name, const void* target) {
    NNH_ASSERT(target != nullptr, "live: \"%.*s\" registered with a null pointer", name.length, name.text);
    if (target == nullptr || name.text == nullptr) return nullptr;
    Registry& r = registry();
    return name_usable(r, name.text, name.length) ? &r : nullptr;
}

/// A new entry with the common fields, or null.
Entry* add(Registry& r, const nnh::live::Name& name, Kind kind, void* target, nnh::live::OnChange on_change) {
    const int slot = add_tunable(r, name.text, name.length, name.hash);
    if (slot < 0) return nullptr;
    Entry& e = r.entries[slot];
    e.kind = kind;
    e.target = target;
    e.callback = on_change.fn;
    e.context = on_change.context;
    return &e;
}

/// The entry's id, the PC told about it when attached (LiveEntry).
std::uint32_t id_of(const Registry& r, const Entry* e) {
    if (e == nullptr) return 0;
    const int slot = static_cast<int>(e - r.entries);
    if (g_on_added != nullptr) g_on_added(slot, false);
    return make_id(0, slot, e->generation);
}

[[maybe_unused]] const char* range_text(RangeError e) {
    switch (e) {
    case RangeError::NotFinite: return "a bound is not finite";
    case RangeError::Empty: return "min must be below max";
    case RangeError::BadStep: return "step must be above 0";
    case RangeError::TooManySteps: return "more than 2^24 steps";
    default: return "";
    }
}

std::uint32_t now() { return static_cast<std::uint32_t>(nnh::frame_count()); }

} // namespace

std::uint32_t tune_float(const nnh::live::Name& name, float* value, const nnh::live::FloatRange& range,
                         nnh::live::OnChange on_change) {
    FloatGrid g;
    const RangeError bad = make_float_grid(range.min, range.max, range.step, g);
    NNH_ASSERT(bad == RangeError::None, "live: \"%.*s\": %s", name.length, name.text, range_text(bad));
    if (bad != RangeError::None) return 0;
    Registry* r = start(name, value);
    if (r == nullptr) return 0;
    const float v = *value;
    NNH_ASSERT(std::isfinite(v) && v >= range.min && v <= range.max, "live: \"%.*s\": default %g outside %g..%g",
               name.length, name.text, static_cast<double>(v), static_cast<double>(range.min),
               static_cast<double>(range.max));
    if (!(std::isfinite(v) && v >= range.min && v <= range.max)) return 0;
    Entry* e = add(*r, name, Kind::Float, value, on_change);
    if (e == nullptr) return 0;
    e->min = float_bits(range.min);
    e->max = float_bits(range.max);
    e->step = float_bits(range.step);
    e->def = read_bits(*e);
    return id_of(*r, e);
}

std::uint32_t tune_int(const nnh::live::Name& name, void* target, const nnh::live::IntRange& range,
                       nnh::live::OnChange on_change) {
    IntGrid g;
    const RangeError bad = make_int_grid(range.min, range.max, range.step, g);
    NNH_ASSERT(bad == RangeError::None, "live: \"%.*s\": %s", name.length, name.text, range_text(bad));
    if (bad != RangeError::None) return 0;
    Registry* r = start(name, target);
    if (r == nullptr) return 0;
    std::int32_t v = 0;
    std::memcpy(&v, target, 4);
    const std::int32_t* value = &v;
    NNH_ASSERT(*value >= range.min && *value <= range.max, "live: \"%.*s\": default %ld outside %ld..%ld", name.length,
               name.text, static_cast<long>(*value), static_cast<long>(range.min), static_cast<long>(range.max));
    if (*value < range.min || *value > range.max) return 0;
    Entry* e = add(*r, name, Kind::Int32, target, on_change);
    if (e == nullptr) return 0;
    e->min = static_cast<std::uint32_t>(range.min);
    e->max = static_cast<std::uint32_t>(range.max);
    e->step = static_cast<std::uint32_t>(range.step);
    e->def = read_bits(*e);
    return id_of(*r, e);
}

std::uint32_t tune_bool(const nnh::live::Name& name, bool* value, nnh::live::OnChange on_change) {
    Registry* r = start(name, value);
    if (r == nullptr) return 0;
    Entry* e = add(*r, name, Kind::Bool, value, on_change);
    if (e == nullptr) return 0;
    e->max = 1;
    e->def = read_bits(*e);
    return id_of(*r, e);
}

std::uint32_t tune_color(const nnh::live::Name& name, nnh::Color* value, nnh::live::OnChange on_change) {
    Registry* r = start(name, value);
    if (r == nullptr) return 0;
    Entry* e = add(*r, name, Kind::Color, value, on_change);
    if (e == nullptr) return 0;
    e->max = 0xFFFFFFFFu;
    e->def = read_bits(*e);
    return id_of(*r, e);
}

std::uint32_t tune_enum(const nnh::live::Name& name, void* value, int value_bytes, const char* const* names, int count,
                        nnh::live::OnChange on_change) {
    bool names_ok = names != nullptr && count >= 1 && count <= nnh::live::kMaxEnumNames;
    for (int i = 0; names_ok && i < count; ++i) {
        const std::size_t n = names[i] == nullptr ? 0 : std::strlen(names[i]);
        names_ok = n >= 1 && n <= 15;
    }
    NNH_ASSERT(names_ok, "live: \"%.*s\": an enum needs 1..32 names of 1..15 bytes", name.length, name.text);
    NNH_ASSERT(value_bytes == 1 || value_bytes == 2 || value_bytes == 4, "live: enums of 1, 2 or 4 bytes");
    if (!names_ok || (value_bytes != 1 && value_bytes != 2 && value_bytes != 4)) return 0;
    Registry* r = start(name, value);
    if (r == nullptr) return 0;
    Entry* e = add(*r, name, Kind::Enum, value, on_change);
    if (e == nullptr) return 0;
    e->value_bytes = static_cast<std::uint8_t>(value_bytes);
    e->enum_names = names;
    e->enum_count = static_cast<std::uint8_t>(count);
    e->max = static_cast<std::uint32_t>(count - 1);
    e->def = read_bits(*e);
    NNH_ASSERT(e->def < static_cast<std::uint32_t>(count), "live: \"%.*s\": enum value %u outside 0..%d", name.length,
               name.text, static_cast<unsigned>(e->def), count - 1);
    if (e->def >= static_cast<std::uint32_t>(count)) {
        remove_tunable(*r, static_cast<int>(e - r->entries));
        return 0;
    }
    return id_of(*r, e);
}

std::uint32_t action(const nnh::live::Name& name, void (*fn)(void*), void* context) {
    Registry* r = start(name, reinterpret_cast<const void*>(fn));
    if (r == nullptr) return 0;
    Entry* e = add(*r, name, Kind::Action, nullptr, {fn, context});
    return id_of(*r, e);
}

void remove(std::uint32_t id) {
    if (g_registry == nullptr) return; // the engine shut down first: nothing left to remove
    Registry& r = *g_registry;
    if (id_is_watch(id)) {
        if (watch_of(r, id) != nullptr) remove_watch(r, id_slot(id));
    } else if (tunable_of(r, id) != nullptr) {
        remove_tunable(r, id_slot(id));
    }
}

bool changed(std::uint32_t id) {
    Entry* e = g_registry != nullptr ? tunable_of(*g_registry, id) : nullptr;
    NNH_ASSERT(e != nullptr, "live: changed() on a handle without an entry (moved from, or after shutdown)");
    return e != nullptr && (e->flags & kChanged) != 0 && e->changed_frame == now();
}

void reset(std::uint32_t id) {
    Entry* e = g_registry != nullptr ? tunable_of(*g_registry, id) : nullptr;
    NNH_ASSERT(e != nullptr, "live: reset() on a handle without an entry (moved from, or after shutdown)");
    if (e != nullptr) reset_now(*e);
}

void reset_all() {
    if (g_registry == nullptr) return;
    for (int i = 0; i < g_registry->tunable_count; ++i) reset_now(g_registry->entries[g_registry->tunable_index[i]]);
}

void reset_group(const nnh::live::Name& group) {
    if (g_registry == nullptr || group.text == nullptr) return;
    Registry& r = *g_registry;
    for (int i = 0; i < r.tunable_count; ++i) {
        Entry& e = r.entries[r.tunable_index[i]];
        if (in_group(e.name, e.name_length, group.text, group.length)) reset_now(e);
    }
}

void configure(int max_tunables, int max_watches, int max_shown) {
    set_capacities(max_tunables, max_watches, max_shown);
}

} // namespace nnh::detail::live_api
