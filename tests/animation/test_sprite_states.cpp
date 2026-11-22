// The state machine's data (T5.1; FR-12, FR-13): states, next-on-finish, rules, errors, and rule priority.

#include <cstring>

#include "library_fixture.hpp"

using namespace nnh::detail::spritefmt;
using nnh::SpriteLibrary;
using nnh::SpriteStateId;
using nnh::SpriteSwitch;

NNH_TEST(states_are_named_and_found) {
    SpriteLibrary lib = anim_test::strip(4);
    const nnh::SpriteClipId idle = anim_test::clip(lib, "idle", 1, 6);
    auto s = lib.add_state("idle", idle);
    CHECK(s.ok() && s.value().value == 0 && lib.state_count() == 1);
    CHECK(lib.find_state("idle") == s.value());
    CHECK(!lib.find_state("run").valid());
    CHECK(std::strcmp(lib.state_name(s.value()), "idle") == 0);
    CHECK(std::strcmp(lib.state_name({9}), "") == 0);
}

NNH_TEST(state_errors_are_returned) {
    auto made = SpriteLibrary::create({.frames = 1, .clips = 1, .clip_frames = 1, .states = 1, .transitions = 1});
    SpriteLibrary lib = std::move(made.value());
    CHECK(lib.add_frame({.region = {0, 0, 8, 8}}).ok());
    const nnh::SpriteClipId c = lib.add_clip({.name = "c", .count = 1}).value();
    CHECK(!lib.add_state("a", {3}).ok());
    CHECK(!lib.add_state("", c).ok());
    const SpriteStateId a = lib.add_state("a", c).value();
    CHECK(!lib.add_state("a", c).ok());
    auto full = lib.add_state("b", c);
    CHECK(!full.ok() && full.error().code == nnh::ErrorCode::LimitReached);
    CHECK(!lib.set_next_on_finish({4}, a).ok());
    CHECK(!lib.set_next_on_finish(a, {4}).ok());
    CHECK(lib.set_next_on_finish(a, a).ok());
    CHECK(lib.set_next_on_finish(a, {}).ok()); // none
    CHECK(!lib.add_transition({.from = {4}}).ok());
    CHECK(!lib.add_transition({.when = static_cast<SpriteSwitch>(5)}).ok());
    CHECK(lib.add_transition({.from = a, .when = SpriteSwitch::Never}).ok());
    CHECK(!lib.add_transition({.from = a, .when = SpriteSwitch::AtEnd}).ok()); // same from/to: duplicate
    auto rules_full = lib.add_transition({.to = a});
    CHECK(!rules_full.ok() && rules_full.error().code == nnh::ErrorCode::LimitReached);
}

NNH_TEST(the_most_specific_rule_wins) {
    const TransitionEntry rules[] = {
        {kAnyState, kAnyState, kNever, 0},
        {kAnyState, 2, kAtEnd, 0},
        {1, kAnyState, kNow, 1},
        {1, 2, kNever, 0},
    };
    CHECK(nnh::detail::sprite_resolve_rule(rules, 4, 1, 2).when == kNever);    // from→to
    CHECK(nnh::detail::sprite_resolve_rule(rules, 4, 1, 3).keep_time == 1);    // from→any
    CHECK(nnh::detail::sprite_resolve_rule(rules, 4, 0, 2).when == kAtEnd);    // any→to
    CHECK(nnh::detail::sprite_resolve_rule(rules, 4, 0, 3).when == kNever);    // any→any
    CHECK(nnh::detail::sprite_resolve_rule(rules, 3, 1, 2).when == kNow);      // from→any beats any→to
    CHECK(nnh::detail::sprite_resolve_rule(rules, 1, 0, 3).when == kNever);
    CHECK(nnh::detail::sprite_resolve_rule(rules, 0, 0, 3).when == kNow);      // no rule: now
    const TransitionEntry any_to_first[] = {{kAnyState, 2, kNever, 0}, {kAnyState, kAnyState, kAtEnd, 0}};
    CHECK(nnh::detail::sprite_resolve_rule(any_to_first, 2, 0, 2).when == kNever); // any→to beats any→any
}

NNH_TEST(of_two_rules_of_the_same_shape_the_first_wins) {
    const TransitionEntry rules[] = {
        {3, 4, kNever, 0}, // other states: never matches
        {1, kAnyState, kAtEnd, 0},
        {1, kAnyState, kNever, 0},
        {kAnyState, kAnyState, kNow, 1},
    };
    CHECK(nnh::detail::sprite_resolve_rule(rules, 4, 1, 2).when == kAtEnd);
    CHECK(nnh::detail::sprite_resolve_rule(rules, 4, 0, 2).keep_time == 1);
}
