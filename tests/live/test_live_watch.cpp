// Watches (spec 030 T2.4, FR-17, FR-19): every kind's bytes, registered watches read only when asked, show()'s latest
// value, text cut at 31 bytes, expiry after 120 frames unseen.

#include <cstring>

#include "live/live_registry.hpp"
#include "live/live_watch.hpp"
#include "live_fixture.hpp"
#include "nnh/gfx.hpp"

using namespace nnh::detail::live;
using live_test::frames;
using live_test::restart;
namespace live = nnh::live;

namespace {

std::uint32_t word(const WatchValue& v) {
    std::uint32_t b = 0;
    std::memcpy(&b, v.bytes, 4);
    return b;
}

WatchValue read(const live::Handle& h) { return read_watch(*watch_of(*g_registry, h.id())); }

const ShownEntry* shown(const char* name) {
    const int slot = find_shown(*g_registry, nnh::detail::fnv1a32(name));
    return slot < 0 ? nullptr : &g_registry->shown[slot];
}

} // namespace

NNH_TEST(live_watch_every_kind_gives_its_bytes) {
    if constexpr (!live::kEnabled) return;
    restart();
    bool b = true;
    std::int8_t i8 = -2;
    std::int16_t i16 = -300;
    int i32 = -70000;
    std::uint8_t u8 = 200;
    std::uint16_t u16 = 60000;
    std::uint32_t u32 = 4000000000u;
    float f = 2.5f;
    nnh::Vec2 v{12.5f, 40.0f};
    nnh::Color c{1, 2, 3, 4};
    live::Handle hb = live::watch("w/b", &b), h8 = live::watch("w/i8", &i8), h16 = live::watch("w/i16", &i16),
                 h32 = live::watch("w/i32", &i32), hu8 = live::watch("w/u8", &u8), hu16 = live::watch("w/u16", &u16),
                 hu32 = live::watch("w/u32", &u32), hf = live::watch("w/f", &f), hv = live::watch("w/v", &v),
                 hc = live::watch("w/c", &c);
    CHECK(read(hb).kind == Kind::Bool && word(read(hb)) == 1);
    CHECK(read(h8).kind == Kind::Int8 && word(read(h8)) == 0xFFFFFFFEu);
    CHECK(read(h16).kind == Kind::Int16 && word(read(h16)) == static_cast<std::uint32_t>(-300));
    CHECK(read(h32).kind == Kind::Int32 && word(read(h32)) == static_cast<std::uint32_t>(-70000));
    CHECK(read(hu8).kind == Kind::UInt8 && word(read(hu8)) == 200);
    CHECK(read(hu16).kind == Kind::UInt16 && word(read(hu16)) == 60000);
    CHECK(read(hu32).kind == Kind::UInt32 && word(read(hu32)) == 4000000000u);
    CHECK(read(hf).kind == Kind::Float && word(read(hf)) == 0x40200000u);
    const WatchValue rv = read(hv);
    CHECK(rv.kind == Kind::Vec2 && rv.length == 8 && std::memcmp(rv.bytes, &v, 8) == 0);
    CHECK(read(hc).kind == Kind::Color && word(read(hc)) == 0x04030201u);
}

NNH_TEST(live_watch_text_through_a_pointer_and_an_array) {
    if constexpr (!live::kEnabled) return;
    restart();
    const char* state = "idle";
    char label[40] = "a label";
    live::Handle hp = live::watch("w/state", &state);
    live::Handle ha = live::watch("w/label", label);
    CHECK(read(hp).kind == Kind::Text && read(hp).length == 4 && std::memcmp(read(hp).bytes, "idle", 4) == 0);
    state = "jumping";
    CHECK(read(hp).length == 7);
    std::memset(label, 'x', sizeof label - 1);
    label[39] = 0;
    CHECK(read(ha).length == 31); // cut at 31 bytes
}

NNH_TEST(live_watch_registered_values_are_read_only_when_asked) {
    if constexpr (!live::kEnabled) return;
    restart();
    float f = 0;
    live::Handle h = live::watch("w/f", &f);
    const std::uint32_t reads = g_watch_reads;
    frames(200);
    CHECK(g_watch_reads == reads);
    (void)read(h);
    CHECK(g_watch_reads == reads + 1);
}

NNH_TEST(live_watch_show_keeps_the_latest_value) {
    if constexpr (!live::kEnabled) return;
    restart();
    live::show("enemies/alive", 3);
    live::show("enemies/alive", 9);
    const ShownEntry* s = shown("enemies/alive");
    CHECK(s != nullptr && s->kind == Kind::Int32);
    std::uint32_t b = 0;
    std::memcpy(&b, s->value, 4);
    CHECK(b == 9);
    CHECK(g_registry->shown_count == 1);
    live::show("enemies/where", nnh::Vec2{1, 2});
    live::show("enemies/state", "chasing");
    CHECK(g_registry->shown_count == 3);
    CHECK(shown("enemies/state")->length == 7);
}

NNH_TEST(live_watch_show_cuts_long_text_and_warns_once) {
    if constexpr (!live::kEnabled) return;
    restart();
    live::show("t/long", "0123456789012345678901234567890123456789");
    CHECK(shown("t/long")->length == 31);
    CHECK(g_registry->text_cut_warned);
}

NNH_TEST(live_watch_shown_values_expire_after_120_frames_unseen) {
    if constexpr (!live::kEnabled) return;
    restart();
    live::show("a/x", 1);
    frames(1);
    for (int i = 0; i < 150; ++i) {
        live::show("a/y", i); // kept alive
        frames(1);
    }
    CHECK(shown("a/x") == nullptr);
    CHECK(shown("a/y") != nullptr);
    CHECK(g_registry->shown_count == 1);
}

NNH_TEST(live_watch_a_full_shown_table_asserts) {
    if constexpr (!live::kEnabled) return;
    restart();
    live::configure({.max_tunables = 4, .max_watches = 1, .max_shown = 2});
    live::show("s/a", 1);
    live::show("s/b", 2);
    CHECK(live_test::asserts_or_is_ignored([] { live::show("s/c", 3); }));
}
