// The prompt helper (spec 028 T6.5, FR-8): a modal of a message and 1..3 buttons built from the primitives, its
// default and cancel answers, the answer as an Activated result with the button's key, closed on answer (focus given
// back), close_prompt by hand.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;
using ui_test::K;
using ui_test::kConfirm;
using ui_test::kLeft;
using ui_test::tap;

namespace {

struct Title {
    Ui ui = ui_test::make();
    UiWidget quit;
    Title() {
        quit = ui.button(ui.root(Screen::Bottom), {.text = "Quit"});
        ui_test::settle(ui);
    }
    UiWidget ask(bool close = true) {
        return ui.prompt(Screen::Bottom, {.message = "Quit the game?", .buttons = {"Yes", "No"}, .keys = {K::Yes, K::No},
                                          .default_button = 1, .cancel_button = 1, .close_on_answer = close});
    }
};

} // namespace

NNH_TEST(ui_prompt_opens_modally_on_its_default_button) {
    Title t;
    const UiWidget p = t.ask();
    CHECK(t.ui.alive(p) && t.ui.modal_depth() == 1 && t.ui.focused_key() == nnh::UiKey(K::No));
    CHECK(t.ui.kind(p) == nnh::UiKind::Panel);
    const nnh::UiRect r = t.ui.rect(p);
    CHECK(r.w == 240 && r.x == 40); // 3/4 of 320, centred
}

NNH_TEST(ui_prompt_answers_by_key_and_closes) {
    Title t;
    const UiWidget p = t.ask();
    t.ui.update(nnh::UiInput{});
    tap(t.ui, kLeft);
    tap(t.ui, kConfirm);
    CHECK(t.ui.activated(K::Yes));
    CHECK(!t.ui.alive(p) && t.ui.modal_depth() == 0 && t.ui.focused() == t.quit);
}

NNH_TEST(ui_prompt_cancel_gives_the_cancel_answer) {
    Title t;
    t.ask();
    t.ui.update(nnh::UiInput{});
    tap(t.ui, ui_test::kCancel);
    CHECK(t.ui.activated(K::No) && t.ui.cancelled() && t.ui.modal_depth() == 0);
}

NNH_TEST(ui_prompt_kept_open_until_closed_by_hand) {
    Title t;
    const UiWidget p = t.ask(false);
    t.ui.update(nnh::UiInput{});
    tap(t.ui, kConfirm);
    CHECK(t.ui.activated(K::No) && t.ui.alive(p) && t.ui.modal_depth() == 1);
    t.ui.close_prompt(p);
    CHECK(!t.ui.alive(p) && t.ui.modal_depth() == 0 && t.ui.focused() == t.quit);
    CHECK(t.ui.stats().widgets == 3);
}

NNH_TEST(ui_prompt_one_to_three_buttons) {
    static Ui* g;
    Title t;
    g = &t.ui;
    const UiWidget one = t.ui.prompt(Screen::Bottom, {.message = "Saved.", .buttons = {"OK"}, .count = 1,
                                                      .cancel_button = 0});
    CHECK(t.ui.alive(one));
    t.ui.close_prompt(one);
    CHECK(ui_test::asserts_or_is_ignored([] { (void)g->prompt(Screen::Bottom, {.count = 4}); }));
    CHECK(ui_test::asserts_or_is_ignored([] { (void)g->prompt(Screen::Bottom, {.count = 2, .default_button = 2}); }));
}
