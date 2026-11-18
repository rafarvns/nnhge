// The voice table (audio/voice_table.hpp, spec 008 FR-2, FR-5): handles with generations, a free voice
// first, else replacing the lowest priority then the oldest, and dropping a sound below them all.

#include "audio/voice_table.hpp"
#include "test.hpp"

using nnh::Voice;
using nnh::detail::VoiceState;
using nnh::detail::VoiceTable;

namespace {

VoiceState g_voices[4];

VoiceTable table_of(int count) {
    VoiceTable t;
    t.reset(g_voices, count);
    return t;
}

} // namespace

NNH_TEST(voice_table_hands_out_free_voices_first) {
    VoiceTable t = table_of(3);
    const int a = t.choose(128);
    CHECK(a >= 0);
    const Voice va = t.claim(a, 128);
    CHECK(va.generation != 0 && va.index == a);
    const int b = t.choose(128);
    CHECK(b >= 0 && b != a);
    t.claim(b, 128);
    CHECK(t.active_count() == 2);
    CHECK(t.get(va) == &t.at(a));
}

NNH_TEST(voice_table_released_handles_go_stale) {
    VoiceTable t = table_of(2);
    const Voice v = t.claim(t.choose(10), 10);
    t.release(v.index);
    CHECK(t.get(v) == nullptr);
    CHECK(t.active_count() == 0);
    const Voice again = t.claim(t.choose(10), 10); // may reuse the index: a new generation
    CHECK(!(again == v));
    CHECK(t.get(v) == nullptr && t.get(again) != nullptr);
    CHECK(t.get(Voice{}) == nullptr);
    CHECK(t.get(Voice{9, 1}) == nullptr); // index out of range
}

NNH_TEST(voice_table_replaces_the_lowest_priority_then_the_oldest) {
    VoiceTable t = table_of(3);
    const Voice low_old = t.claim(t.choose(50), 50);
    const Voice high = t.claim(t.choose(200), 200);
    const Voice low_new = t.claim(t.choose(50), 50);
    // Full: a sound of priority 100 takes the oldest of the lowest priority.
    CHECK(t.choose(100) == low_old.index);
    // Equal priority may replace too (the oldest one).
    CHECK(t.choose(50) == low_old.index);
    const Voice replacing = t.claim(low_old.index, 100);
    CHECK(t.get(low_old) == nullptr && t.get(replacing) != nullptr);
    CHECK(t.choose(60) == low_new.index);
    (void)high;
}

NNH_TEST(voice_table_drops_a_sound_below_every_busy_priority) {
    VoiceTable t = table_of(2);
    t.claim(t.choose(100), 100);
    t.claim(t.choose(120), 120);
    CHECK(t.choose(99) == -1);
    CHECK(t.choose(100) >= 0);
}

NNH_TEST(voice_table_generations_skip_zero_when_they_wrap) {
    VoiceTable t = table_of(1);
    Voice last{};
    for (int i = 0; i < 70000; ++i) {
        last = t.claim(0, 1);
        CHECK(last.generation != 0);
        t.release(0);
        if (last.generation == 0) break;
    }
}

NNH_TEST(voice_table_reset_forgets_everything) {
    VoiceTable t = table_of(2);
    const Voice v = t.claim(t.choose(1), 1);
    t.reset(g_voices, 2);
    CHECK(t.active_count() == 0);
    CHECK(t.get(v) == nullptr);
}
