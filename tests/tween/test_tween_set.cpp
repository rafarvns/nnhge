// Tween set storage (spec 015 FR-4, FR-18; T2.1): one block per set under the memory tag `tween`, freed on
// destruction, moved with the set, capacities checked.

#include "nnh/detail/target_layout.hpp"
#include "nnh/memory.hpp"
#include "tween/tween_block.hpp"
#include "tween/tween_internal.hpp"
#include "tween_fixture.hpp"

#include <utility>

using nnh::TweenSet;

namespace {

nnh::MemStats tag() {
    return nnh::mem::tag_stats(nnh::detail::tween_memory_tag());
}

void create_with_no_tweens() {
    TweenSet s({.tweens = 0});
}
void create_with_too_many_tweens() {
    TweenSet s({.tweens = nnh::kMaxTweens + 1});
}
void create_with_negative_steps() {
    TweenSet s({.tweens = 4, .steps = -1});
}
void create_with_too_many_steps() {
    TweenSet s({.tweens = 4, .steps = nnh::kMaxSteps + 1});
}

} // namespace

NNH_TEST(a_tween_set_reserves_one_block_under_the_tween_tag) {
    const nnh::MemStats before = tag();
    {
        TweenSet set({.tweens = 32, .steps = 16});
        CHECK(tag().count == before.count + 1);
        CHECK(tag().bytes >= before.bytes +
                                 32 * (sizeof(nnh::detail::SlotHot) + sizeof(nnh::detail::SlotCold)) +
                                 16 * sizeof(nnh::detail::Step));
        CHECK(set.capacity() == 32);
        CHECK(set.active() == 0);
    }
    CHECK(tag().count == before.count);
    CHECK(tag().bytes == before.bytes);
}

NNH_TEST(a_default_tween_set_has_no_storage) {
    const nnh::MemStats before = tag();
    TweenSet set;
    CHECK(set.capacity() == 0);
    CHECK(set.active() == 0);
    set.update(); // nothing to do, no crash
    CHECK(tag().count == before.count);
}

NNH_TEST(moving_a_tween_set_moves_its_block) {
    const nnh::MemStats before = tag();
    TweenSet a({.tweens = 8});
    TweenSet b(std::move(a));
    CHECK(b.capacity() == 8);
    CHECK(a.capacity() == 0); // NOLINT(bugprone-use-after-move): moved-from sets are empty
    CHECK(tag().count == before.count + 1);
    TweenSet c({.tweens = 4});
    c = std::move(b); // c's own block is freed
    CHECK(c.capacity() == 8);
    CHECK(tag().count == before.count + 1);
}

NNH_TEST(the_largest_set_fits_the_documented_ceilings) {
    TweenSet set({.tweens = nnh::kMaxTweens, .steps = nnh::kMaxSteps});
    CHECK(set.capacity() == nnh::kMaxTweens);
}

NNH_TEST(one_slot_stays_within_its_size_budget) {
    CHECK(sizeof(nnh::detail::SlotHot) + sizeof(nnh::detail::SlotCold) <=
          (nnh::detail::kThreeDsLayout ? 192u : 256u));
    CHECK(sizeof(nnh::detail::Step) == 8);
    CHECK(sizeof(TweenSet) == sizeof(void*));
}

NNH_TEST(absurd_capacities_assert_or_leave_the_set_empty) {
    CHECK(tween_test::asserts_or_is_ignored(create_with_no_tweens));
    CHECK(tween_test::asserts_or_is_ignored(create_with_too_many_tweens));
    CHECK(tween_test::asserts_or_is_ignored(create_with_negative_steps));
    CHECK(tween_test::asserts_or_is_ignored(create_with_too_many_steps));
    if (!nnh::build::kAssertsEnabled) {
        const nnh::MemStats before = tag();
        TweenSet set({.tweens = 0});
        CHECK(set.capacity() == 0);
        CHECK(tag().count == before.count);
    }
}
