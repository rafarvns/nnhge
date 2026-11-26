// Watches (see live_watch.hpp) and the back ends of live::watch() and live::show().

#include "live/live_watch.hpp"

#include <cstring>

#include "live/live_registry.hpp"
#include "nnh/app.hpp"
#include "nnh/assert.hpp"
#include "nnh/live.hpp"
#include "nnh/log.hpp"

namespace nnh {

NNH_LOG_CHANNEL_EXTERN(live);

namespace detail::live {

std::uint32_t g_watch_reads = 0;

namespace {

/// The text of `text` cut to 31 bytes into `out`; its length. `limit`: bytes readable (an array's size), or -1.
int copy_text(const char* text, int limit, std::uint8_t* out, bool& cut) {
    int n = 0;
    cut = false;
    if (text == nullptr) return 0;
    while ((limit < 0 || n < limit) && text[n] != '\0') {
        if (n == kMaxTextBytes) {
            cut = true;
            break;
        }
        out[n] = static_cast<std::uint8_t>(text[n]);
        ++n;
    }
    return n;
}

bool kind_ok(std::uint8_t k) { return watch_kind(static_cast<Kind>(k)); }

} // namespace

WatchValue read_watch(const WatchEntry& w) {
    ++g_watch_reads;
    WatchValue v;
    v.kind = w.kind;
    if (w.kind == Kind::Text) {
        bool cut = false;
        const char* text = (w.flags & kTextPointer) != 0 ? *static_cast<const char* const*>(w.target)
                                                         : static_cast<const char*>(w.target);
        v.length = static_cast<std::uint8_t>(copy_text(text, (w.flags & kTextPointer) != 0 ? -1 : w.text_bytes,
                                                       v.bytes, cut));
        return v;
    }
    if (w.kind == Kind::Bool) {
        const std::uint32_t b = *static_cast<const bool*>(w.target) ? 1u : 0u;
        std::memcpy(v.bytes, &b, 4);
        v.length = 4;
        return v;
    }
    std::uint32_t b = 0;
    switch (w.kind) {
    case Kind::Int8: b = static_cast<std::uint32_t>(static_cast<std::int32_t>(*static_cast<const std::int8_t*>(w.target))); break;
    case Kind::Int16: b = static_cast<std::uint32_t>(static_cast<std::int32_t>(*static_cast<const std::int16_t*>(w.target))); break;
    case Kind::UInt8: b = *static_cast<const std::uint8_t*>(w.target); break;
    case Kind::UInt16: b = *static_cast<const std::uint16_t*>(w.target); break;
    case Kind::Vec2: std::memcpy(v.bytes, w.target, 8); v.length = 8; return v;
    default: std::memcpy(&b, w.target, 4); break; // float, int32, uint32, colour (r, g, b, a bytes = packed bits)
    }
    std::memcpy(v.bytes, &b, 4);
    v.length = 4;
    return v;
}

WatchValue read_shown(const ShownEntry& s) {
    WatchValue v;
    v.kind = s.kind;
    v.length = s.length;
    std::memcpy(v.bytes, s.value, sizeof v.bytes);
    return v;
}

void sweep_shown(Registry& r, std::uint32_t frame) {
    for (int i = 0; i < r.max_shown; ++i) {
        const ShownEntry& s = r.shown[i];
        if ((s.flags & kUsed) != 0 && frame - s.last_frame >= static_cast<std::uint32_t>(kShownForFrames)) {
            remove_shown(r, i);
        }
    }
}

} // namespace detail::live

namespace detail::live_api {

using namespace nnh::detail::live;

std::uint32_t watch(const nnh::live::Name& name, const void* value, std::uint8_t kind, int text_bytes) {
    NNH_ASSERT(value != nullptr, "live: watch \"%.*s\" with a null pointer", name.length, name.text);
    if (value == nullptr || name.text == nullptr || !kind_ok(kind)) return 0;
    Registry& r = registry();
    if (!name_usable(r, name.text, name.length)) return 0;
    const int slot = add_watch(r, name.text, name.length, name.hash);
    if (slot < 0) return 0;
    WatchEntry& w = r.watches[slot];
    w.target = value;
    w.kind = static_cast<Kind>(kind);
    if (w.kind == Kind::Text) {
        if (text_bytes < 0) w.flags |= kTextPointer;
        else w.text_bytes = static_cast<std::int16_t>(text_bytes);
    }
    return make_id(kWatchTable, slot, w.generation);
}

namespace {

ShownEntry* shown_slot(const nnh::live::Name& name) {
    if (name.text == nullptr) return nullptr;
    Registry& r = registry();
    bool added = false;
    const int slot = find_or_add_shown(r, name.text, name.length, name.hash, added);
    NNH_ASSERT(slot >= 0 || r.shown_count < r.max_shown || r.max_shown == 0,
               "live: more than %d per-frame watches; raise live::configure({.max_shown})", r.max_shown);
    if (slot < 0) return nullptr;
    ShownEntry& s = r.shown[slot];
    s.last_frame = static_cast<std::uint32_t>(nnh::frame_count());
    if (added && r.next_sweep == 0) r.next_sweep = s.last_frame + kSweepEvery;
    if (added && g_on_added != nullptr) g_on_added(slot | kShownRef, true);
    return &s;
}

} // namespace

void show(const nnh::live::Name& name, std::uint8_t kind, const void* value, int bytes) {
    ShownEntry* s = shown_slot(name);
    if (s == nullptr) return;
    s->kind = static_cast<Kind>(kind);
    std::uint32_t b = 0;
    switch (s->kind) {
    case Kind::Bool: b = *static_cast<const bool*>(value) ? 1u : 0u; break;
    case Kind::Int8: b = static_cast<std::uint32_t>(static_cast<std::int32_t>(*static_cast<const std::int8_t*>(value))); break;
    case Kind::Int16: b = static_cast<std::uint32_t>(static_cast<std::int32_t>(*static_cast<const std::int16_t*>(value))); break;
    case Kind::UInt8: b = *static_cast<const std::uint8_t*>(value); break;
    case Kind::UInt16: b = *static_cast<const std::uint16_t*>(value); break;
    case Kind::Vec2:
        std::memcpy(s->value, value, 8);
        s->length = 8;
        return;
    default: std::memcpy(&b, value, bytes < 4 ? static_cast<std::size_t>(bytes) : 4u); break;
    }
    std::memcpy(s->value, &b, 4);
    s->length = 4;
}

void show_text(const nnh::live::Name& name, const char* text) {
    ShownEntry* s = shown_slot(name);
    if (s == nullptr) return;
    s->kind = Kind::Text;
    bool cut = false;
    s->length = static_cast<std::uint8_t>(copy_text(text, -1, s->value, cut));
    if (cut && !g_registry->text_cut_warned) {
        g_registry->text_cut_warned = true;
        NNH_LOG_WARN(live, "show(\"%.*s\"): text longer than 31 bytes, cut", name.length, name.text);
    }
}

} // namespace detail::live_api
} // namespace nnh
