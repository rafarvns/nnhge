// Declaring contexts and actions (spec 017 T2.2, FR-1, FR-2, FR-6, FR-8): names, ids, duplicates, hash
// collisions, capacities, sources that don't fit, touch always locked.

#include "actions_fixture.hpp"

#include <cstring>

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionId;
using nnh::ActionSet;
using nnh::Source;

namespace {
enum class Act { Jump, Run, Move, Extra };
enum class Ctx { Main, Menu };

ActionSet* g_set = nullptr;

void bad_name_upper() { g_set->add(Act::Jump, nnh::ActionDesc{.name = "Jump"}); }
void bad_name_empty() { g_set->add(Act::Jump, nnh::ActionDesc{.name = ""}); }
void bad_name_null() { g_set->add(Act::Jump, nnh::ActionDesc{}); }
void bad_name_long() { g_set->add(Act::Jump, nnh::ActionDesc{.name = "abcdefghijklmnopqrstuvwxyz012345"}); } // 32
void bad_name_space() { g_set->add(Act::Jump, nnh::ActionDesc{.name = "ju mp"}); }
void duplicate_id() { g_set->add(Act::Jump, nnh::ActionDesc{.name = "other"}); }
void duplicate_name() { g_set->add(Act::Run, nnh::ActionDesc{.name = "jump"}); }
void hash_collision() { g_set->add(Act::Run, nnh::ActionDesc{.name = "15xa"}); } // FNV-1a 32 equal to "gv_u"
void id_out_of_range() { g_set->add(ActionId(4), nnh::ActionDesc{.name = "far"}); }
void undeclared_context() { g_set->add(Act::Run, nnh::ActionDesc{.name = "run", .context = Ctx::Menu}); }
void unfit_source() {
    g_set->add(Act::Run, nnh::ActionDesc{.name = "run", .kind = ActionKind::Vector, .bindings = {btn(Button::A)}});
}
void bad_button() {
    g_set->add(Act::Run, nnh::ActionDesc{.name = "run", .bindings = {{Source::button(static_cast<Button>(3))}}});
}
void context_out_of_range() { g_set->add_context(nnh::ContextId(2), "far"); }
void context_bad_name() { g_set->add_context(Ctx::Menu, "Menu"); }
void context_duplicate_name() {
    g_set->add_context(Ctx::Menu, "menu");
    g_set->add_context(nnh::ContextId(2), "menu");
}

} // namespace

NNH_TEST(context_zero_exists_as_default_and_enabled) {
    restart();
    ActionSet set{{.actions = 1, .contexts = 2}};
    CHECK(set.enabled(Ctx::Main));
    CHECK(std::strcmp(block_of(set)->ctx[0].name, "default") == 0);
    CHECK(set.add_context(Ctx::Menu, "menu", false));
    CHECK(!set.enabled(Ctx::Menu));
    nnh::shutdown();
}

NNH_TEST(an_action_copies_its_name) {
    restart();
    ActionSet set{{.actions = 4}};
    char name[8] = "jump";
    CHECK(set.add(Act::Jump, nnh::ActionDesc{.name = name}));
    name[0] = 'x';
    CHECK(std::strcmp(set.name(Act::Jump), "jump") == 0);
    CHECK(set.name(Act::Run)[0] == '\0');
    nnh::shutdown();
}

NNH_TEST(names_of_up_to_31_lowercase_letters_digits_and_underscores_are_accepted) {
    restart();
    ActionSet set{{.actions = 4}};
    CHECK(set.add(Act::Jump, nnh::ActionDesc{.name = "abcdefghijklmnopqrstuvwxyz_0123"}));
    CHECK(set.add(Act::Run, nnh::ActionDesc{.name = "9"}));
    nnh::shutdown();
}

NNH_TEST(bad_names_assert_or_are_refused) {
    restart();
    ActionSet set{{.actions = 4}};
    g_set = &set;
    CHECK(asserts_or_is_ignored(bad_name_upper));
    CHECK(asserts_or_is_ignored(bad_name_empty));
    CHECK(asserts_or_is_ignored(bad_name_null));
    CHECK(asserts_or_is_ignored(bad_name_long));
    CHECK(asserts_or_is_ignored(bad_name_space));
    CHECK(set.name(Act::Jump)[0] == '\0'); // nothing declared
    nnh::shutdown();
}

NNH_TEST(duplicate_ids_names_and_name_hashes_assert_or_are_refused) {
    restart();
    ActionSet set{{.actions = 4}};
    g_set = &set;
    CHECK(set.add(Act::Jump, nnh::ActionDesc{.name = "jump"}));
    CHECK(set.add(Act::Move, nnh::ActionDesc{.name = "gv_u"}));
    CHECK(asserts_or_is_ignored(duplicate_id));
    CHECK(asserts_or_is_ignored(duplicate_name));
    CHECK(asserts_or_is_ignored(hash_collision));
    CHECK(std::strcmp(set.name(Act::Jump), "jump") == 0 && set.name(Act::Run)[0] == '\0');
    nnh::shutdown();
}

NNH_TEST(ids_and_contexts_out_of_range_assert_or_are_refused) {
    restart();
    ActionSet set{{.actions = 4, .contexts = 2}};
    g_set = &set;
    CHECK(asserts_or_is_ignored(id_out_of_range));
    CHECK(asserts_or_is_ignored(undeclared_context));
    CHECK(asserts_or_is_ignored(context_out_of_range));
    CHECK(asserts_or_is_ignored(context_bad_name));
    nnh::shutdown();
}

NNH_TEST(a_duplicate_context_name_asserts_or_is_refused) {
    restart();
    ActionSet set{{.actions = 1, .contexts = 3}};
    g_set = &set;
    CHECK(asserts_or_is_ignored(context_duplicate_name));
    nnh::shutdown();
}

NNH_TEST(sources_that_dont_fit_the_kind_assert_or_are_refused) {
    restart();
    ActionSet set{{.actions = 4}};
    g_set = &set;
    CHECK(asserts_or_is_ignored(unfit_source));
    CHECK(asserts_or_is_ignored(bad_button));
    CHECK(set.name(Act::Run)[0] == '\0');
    nnh::shutdown();
}

NNH_TEST(a_touch_area_is_always_locked) {
    restart();
    ActionSet set{{.actions = 1}};
    CHECK(set.add(Act::Jump, nnh::ActionDesc{.name = "jump", .bindings = {{Source::touch(1, 2, 3, 4)}}}));
    const nnh::detail::SlotRec& s = block_of(set)->actions[0].slots[0];
    CHECK((s.flags & nnh::detail::slot_flag::kLocked) != 0);
    CHECK(set.source(Act::Jump, 0) == Source::touch(1, 2, 3, 4));
    nnh::shutdown();
}

NNH_TEST(declared_bindings_are_the_defaults_and_the_current_sources) {
    restart();
    ActionSet set{{.actions = 4}};
    CHECK(set.add(Act::Move, nnh::ActionDesc{.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(nnh::AnalogStick::CirclePad)}, {Source::dpad()}}}));
    CHECK(set.source(Act::Move, 0) == Source::stick(nnh::AnalogStick::CirclePad));
    CHECK(set.source(Act::Move, 1) == Source::dpad());
    CHECK(set.source(Act::Move, 2).kind == Source::Kind::None);
    const nnh::detail::SlotRec& s = block_of(set)->actions[2].slots[1];
    CHECK(s.cur == s.def);
    nnh::shutdown();
}
