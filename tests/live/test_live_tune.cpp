// Tunables and actions (spec 030 T2.2, FR-1, FR-2, FR-5, FR-6, Q9): defaults from the variable, every kind's entry,
// enum sizes 1/2/4, actions, handle moves, stale use, run-time names copied, resets by the game.

#include <cstring>

#include "live/live_apply.hpp"
#include "live/live_registry.hpp"
#include "live_fixture.hpp"
#include "nnh/gfx.hpp"

using namespace nnh::detail::live;
using live_test::asserts_or_is_ignored;
using live_test::restart;
namespace live = nnh::live;

namespace {

enum class Small : std::uint8_t { A, B, C };
enum class Mid : std::int16_t { A, B, C, D };
enum Plain { kFire, kIce };
constexpr const char* kSmallNames[] = {"a", "b", "c"};
constexpr const char* kMidNames[] = {"one", "two", "three", "four"};
constexpr const char* kPlainNames[] = {"fire", "ice"};

Entry& entry_of(const live::Handle& h) { return *tunable_of(*g_registry, h.id()); }

int g_runs = 0;
void run(void* context) { g_runs += *static_cast<int*>(context); }

} // namespace

NNH_TEST(live_tune_float_keeps_the_variable_and_its_default) {
    if constexpr (!live::kEnabled) return;
    restart();
    float jump = 4.5f;
    live::Handle h = live::tune("player/jump", &jump, {.min = 0, .max = 10, .step = 0.25f});
    CHECK(h.valid());
    const Entry& e = entry_of(h);
    CHECK(e.kind == Kind::Float && e.target == &jump);
    CHECK(e.def == float_bits(4.5f) && e.min == float_bits(0.0f) && e.max == float_bits(10.0f));
    CHECK(e.step == float_bits(0.25f));
    CHECK(std::strcmp(e.name, "player/jump") == 0 && e.name_length == 11);
    CHECK(jump == 4.5f);
}

NNH_TEST(live_tune_every_kind_reads_its_bits) {
    if constexpr (!live::kEnabled) return;
    restart();
    std::int32_t count = -3;
    bool flag = true;
    nnh::Color sky{0xFF, 0xC0, 0x40, 0x80};
    live::Handle hi = live::tune("spawn/count", &count, {.min = -5, .max = 32, .step = 1});
    live::Handle hb = live::tune("debug/hitboxes", &flag);
    live::Handle hc = live::tune("colors/sky", &sky);
    CHECK(entry_of(hi).kind == Kind::Int32 && entry_of(hi).def == static_cast<std::uint32_t>(-3));
    CHECK(entry_of(hb).kind == Kind::Bool && entry_of(hb).def == 1);
    CHECK(entry_of(hc).kind == Kind::Color && entry_of(hc).def == 0x8040C0FFu);
}

NNH_TEST(live_tune_enums_of_every_size) {
    if constexpr (!live::kEnabled) return;
    restart();
    Small s = Small::C;
    Mid m = Mid::B;
    Plain p = kIce;
    live::Handle hs = live::tune("e/small", &s, kSmallNames);
    live::Handle hm = live::tune("e/mid", &m, kMidNames);
    live::Handle hp = live::tune("e/plain", &p, kPlainNames);
    CHECK(entry_of(hs).value_bytes == 1 && entry_of(hs).def == 2 && entry_of(hs).enum_count == 3);
    CHECK(entry_of(hm).value_bytes == 2 && entry_of(hm).def == 1 && entry_of(hm).enum_count == 4);
    CHECK(entry_of(hp).value_bytes == sizeof(Plain) && entry_of(hp).def == 1);
    write_bits(entry_of(hm), 3);
    CHECK(m == Mid::D);
    write_bits(entry_of(hs), 0);
    CHECK(s == Small::A);
}

NNH_TEST(live_tune_enum_default_outside_its_names_asserts) {
    if constexpr (!live::kEnabled) return;
    restart();
    static Mid m = static_cast<Mid>(7);
    CHECK(asserts_or_is_ignored([] { auto h = live::tune("e/mid", &m, kMidNames); }));
}

NNH_TEST(live_tune_actions_have_no_value) {
    if constexpr (!live::kEnabled) return;
    restart();
    int add = 2;
    live::Handle h = live::action("player/respawn", run, &add);
    CHECK(h.valid());
    CHECK(entry_of(h).kind == Kind::Action && entry_of(h).callback == run && entry_of(h).context == &add);
}

NNH_TEST(live_tune_handles_move_and_remove_their_entry) {
    if constexpr (!live::kEnabled) return;
    restart();
    float v = 1;
    live::Handle a = live::tune("m/v", &v, {.min = 0, .max = 2, .step = 1});
    const std::uint32_t id = a.id();
    live::Handle b = static_cast<live::Handle&&>(a);
    CHECK(!a.valid() && b.valid() && b.id() == id);
    CHECK(g_registry->tunable_count == 1);
    live::Handle c;
    c = static_cast<live::Handle&&>(b);
    CHECK(c.id() == id && g_registry->tunable_count == 1);
    c = live::Handle{};
    CHECK(g_registry->tunable_count == 0);
    CHECK(sizeof(live::Handle) == 4);
}

NNH_TEST(live_tune_stale_handles_assert) {
    if constexpr (!live::kEnabled) return;
    restart();
    CHECK(asserts_or_is_ignored([] {
        live::Handle h;
        (void)h.changed();
    }));
    CHECK(asserts_or_is_ignored([] {
        live::Handle h;
        h.reset();
    }));
    CHECK(asserts_or_is_ignored([] {
        static float v = 0;
        live::Handle a = live::tune("s/v", &v, {.min = 0, .max = 1, .step = 1});
        live::Handle b = static_cast<live::Handle&&>(a);
        (void)a.changed(); // moved from
    }));
}

NNH_TEST(live_tune_run_time_names_are_copied) {
    if constexpr (!live::kEnabled) return;
    restart();
    char buffer[32] = "goblin/speed";
    float speed = 1;
    live::Handle h = live::tune(live::Name::runtime(buffer), &speed, {.min = 0, .max = 4, .step = 0.5f});
    std::strcpy(buffer, "xxxxxx/xxxxx");
    CHECK(std::strcmp(entry_of(h).name, "goblin/speed") == 0);
    CHECK(entry_of(h).hash == nnh::detail::fnv1a32("goblin/speed"));
}

NNH_TEST(live_tune_game_resets_write_now_without_changed) {
    if constexpr (!live::kEnabled) return;
    restart();
    float a = 1, b = 2;
    std::int32_t c = 3;
    live::Handle ha = live::tune("p/a", &a, {.min = 0, .max = 9, .step = 1});
    live::Handle hb = live::tune("p/b", &b, {.min = 0, .max = 9, .step = 1});
    live::Handle hc = live::tune("q/c", &c, {.min = 0, .max = 9, .step = 1});
    a = 5;
    b = 6;
    c = 7;
    ha.reset();
    CHECK(a == 1 && !ha.changed());
    live::reset_group("p");
    CHECK(b == 2 && c == 7);
    live::reset_all();
    CHECK(c == 3);
}

NNH_TEST(live_tune_bad_ranges_and_defaults_assert) {
    if constexpr (!live::kEnabled) return;
    restart();
    static float v = 5;
    static std::int32_t i = 50;
    CHECK(asserts_or_is_ignored([] { auto h = live::tune("r/a", &v, {.min = 1, .max = 1, .step = 1}); }));
    CHECK(asserts_or_is_ignored([] { auto h = live::tune("r/b", &v, {.min = 0, .max = 1, .step = 0}); }));
    CHECK(asserts_or_is_ignored([] { auto h = live::tune("r/c", &v, {.min = 0, .max = 1, .step = 0.5f}); }));
    CHECK(asserts_or_is_ignored([] { auto h = live::tune("r/d", &i, {.min = 0, .max = 10, .step = 1}); }));
    CHECK(asserts_or_is_ignored([] { auto h = live::tune("r/e", static_cast<float*>(nullptr), {.min = 0, .max = 1, .step = 1}); }));
    CHECK(asserts_or_is_ignored([] { auto h = live::action("r/f", nullptr); }));
}
