// The set's life (spec 017 T2.1, FR-1): one block under the `actions` tag, ids, move, destroy, empty sets.

#include "actions_fixture.hpp"
#include "nnh/memory.hpp"

#include <utility>

using namespace actions_test;
using nnh::ActionSet;

namespace {
enum class Act { Jump, Run };

void zero_actions() { ActionSet s{{.actions = 0}}; }
void too_many_actions() { ActionSet s{{.actions = nnh::kMaxActions + 1}}; }
void zero_contexts() { ActionSet s{{.actions = 1, .contexts = 0}}; }
void too_many_contexts() { ActionSet s{{.actions = 1, .contexts = nnh::kMaxContexts + 1}}; }
} // namespace

NNH_TEST(an_empty_set_is_not_valid_and_reads_idle) {
    const ActionSet set;
    CHECK(!set.valid() && set.id() == 0);
    CHECK(!set.held(Act::Jump) && !set.pressed(Act::Jump) && set.value(Act::Jump) == 0.0f);
    CHECK(set.held_frames(Act::Jump) == 0 && set.name(Act::Jump)[0] == '\0');
}

NNH_TEST(a_set_reserves_one_block_under_the_actions_tag_and_frees_it) {
    restart();
    const int live = nnh::mem::live_allocations();
    const std::size_t bytes = nnh::mem::tag_stats(nnh::detail::actions_memory_tag()).bytes;
    {
        ActionSet set{{.actions = 16, .contexts = 2}};
        CHECK(set.valid());
        CHECK(nnh::mem::live_allocations() == live + 1);
        const std::size_t used = nnh::mem::tag_stats(nnh::detail::actions_memory_tag()).bytes - bytes;
        CHECK(used >= 16 * sizeof(nnh::detail::ActionRec) + nnh::kMaxBindingsSize);
        CHECK(used <= 16 * sizeof(nnh::detail::ActionRec) + nnh::kMaxBindingsSize + 2048);
    }
    CHECK(nnh::mem::live_allocations() == live);
    CHECK(nnh::mem::tag_stats(nnh::detail::actions_memory_tag()).bytes == bytes);
    nnh::shutdown();
}

NNH_TEST(an_action_record_stays_within_its_documented_size) {
    CHECK(sizeof(nnh::detail::ActionRec) <= 160);
}

NNH_TEST(invalid_capacities_assert_or_give_an_empty_set) {
    restart();
    CHECK(asserts_or_is_ignored(zero_actions));
    CHECK(asserts_or_is_ignored(too_many_actions));
    CHECK(asserts_or_is_ignored(zero_contexts));
    CHECK(asserts_or_is_ignored(too_many_contexts));
    nnh::shutdown();
}

NNH_TEST(sets_have_distinct_non_zero_ids) {
    restart();
    ActionSet a{{.actions = 1}}, b{{.actions = 1}};
    CHECK(a.id() != 0 && b.id() != 0 && a.id() != b.id());
    nnh::shutdown();
}

NNH_TEST(moving_a_set_keeps_its_actions_and_state) {
    restart();
    ActionSet a{{.actions = 2}};
    CHECK(a.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A)}}));
    const std::uint16_t id = a.id();
    step(a, 0);
    step(a, bit(Button::A));
    ActionSet b = std::move(a);
    CHECK(!a.valid() && b.valid() && b.id() == id);
    CHECK(b.pressed(Act::Jump));
    ActionSet c{{.actions = 1}};
    c = std::move(b);
    CHECK(c.id() == id && c.held(Act::Jump));
    nnh::shutdown();
}
