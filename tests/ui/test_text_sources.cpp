// Text sources (spec 028 T2.1, FR-24): copies (capacity), borrowed buffers seen after mark_text_changed, keys resolved
// once until text_changed(), names hashed, a null resolution measuring 0 and drawing nothing.

#include <cstring>

#include "nnh/detail/name_hash.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiText;
using nnh::UiWidget;

namespace {
Ui* g_ui = nullptr;
int g_calls = 0;
const char* g_answer = "Hello";
const char* resolve(std::uint32_t key) {
    ++g_calls;
    return key == 7 ? g_answer : nullptr;
}
} // namespace

NNH_TEST(ui_text_copies_need_room_for_their_bytes) {
    Ui ui = ui_test::make({.text_bytes = 8});
    g_ui = &ui;
    CHECK(ui.alive(ui.label(ui.root(Screen::Top), {.text = "abcdefg"}))); // 8 bytes with the NUL
    CHECK(ui_test::asserts_or_is_ignored([] { (void)g_ui->label(g_ui->root(Screen::Top), {.text = "x"}); }));
    CHECK(ui.stats().text_bytes == 8 && ui.stats().widgets == 3);
}

NNH_TEST(ui_text_a_copy_doesnt_follow_its_source) {
    Ui ui = ui_test::make();
    char buffer[8] = "one";
    const UiWidget w = ui.label(ui.root(Screen::Top), {.text = buffer});
    std::strcpy(buffer, "two");
    CHECK(std::strcmp(ui.text(w), "one") == 0);
}

NNH_TEST(ui_text_borrowed_edits_are_seen_after_mark_text_changed) {
    Ui ui = ui_test::make();
    char buffer[16] = "one";
    const UiWidget w = ui.label(ui.root(Screen::Top), {.text = UiText::borrow(buffer)});
    CHECK(ui.rect(w).w == 24);
    std::strcpy(buffer, "three");
    CHECK(ui.rect(w).w == 24); // not measured again yet
    ui.mark_text_changed(w);
    CHECK(ui.rect(w).w == 40);
    CHECK(ui.text(w) == buffer);
    CHECK(ui.stats().text_bytes == 0);
}

NNH_TEST(ui_text_keys_are_resolved_once_until_text_changed) {
    Ui ui = ui_test::make();
    g_calls = 0;
    g_answer = "Hello";
    ui.set_resolver(&resolve);
    const UiWidget w = ui.label(ui.root(Screen::Top), {.text = UiText::key(7)});
    CHECK(ui.rect(w).w == 40);
    const int calls = g_calls;
    CHECK(calls == 1);
    ui.update(nnh::UiInput{});
    (void)ui.rect(w);
    (void)ui.text(w);
    CHECK(g_calls == calls);
    g_answer = "Bonjour!";
    ui.text_changed();
    CHECK(ui.rect(w).w == 64);
    CHECK(g_calls == calls + 1);
}

NNH_TEST(ui_text_a_null_resolution_measures_nothing) {
    Ui ui = ui_test::make();
    ui.set_resolver(&resolve);
    const UiWidget w = ui.label(ui.root(Screen::Top), {.text = UiText::key(99)});
    CHECK(ui.rect(w).w == 0 && ui.rect(w).h == 0);
    CHECK(ui.text(w)[0] == 0);
}

NNH_TEST(ui_text_without_a_resolver_keys_are_empty) {
    Ui ui = ui_test::make();
    const UiWidget w = ui.label(ui.root(Screen::Top), {.text = UiText::key(7)});
    CHECK(ui.rect(w).w == 0);
}

NNH_TEST(ui_text_names_hash_with_fnv1a) {
    constexpr UiText t = UiText::name("menu.new_game");
    static_assert(t.kind() == UiText::Kind::Key);
    CHECK(t.key_value() == nnh::detail::fnv1a32("menu.new_game"));
    enum class Line : std::uint32_t { Hello = 5 };
    CHECK(UiText::key(Line::Hello).key_value() == 5);
}

NNH_TEST(ui_text_set_text_replaces_a_copy_and_frees_its_bytes) {
    Ui ui = ui_test::make();
    const UiWidget a = ui.label(ui.root(Screen::Top), {.text = "first"});
    const UiWidget b = ui.label(ui.root(Screen::Top), {.text = "keep"});
    ui.set_text(a, "second!");
    CHECK(std::strcmp(ui.text(a), "second!") == 0 && std::strcmp(ui.text(b), "keep") == 0);
    CHECK(ui.stats().text_bytes == 8 + 5);
    ui.set_text(a, UiText::borrow("lit"));
    CHECK(ui.stats().text_bytes == 5 && std::strcmp(ui.text(b), "keep") == 0);
    CHECK(ui.rect(a).w == 24);
}

NNH_TEST(ui_text_choice_options_are_text_sources) {
    Ui ui = ui_test::make({.texts = 3});
    g_answer = "Hello";
    ui.set_resolver(&resolve);
    static const UiText opts[3] = {"Easy", UiText::key(7), UiText::borrow("Hard")};
    const UiWidget c = ui.choice(ui.root(Screen::Bottom), {.text = "", .options = opts, .count = 3});
    CHECK(ui.alive(c));
    // widest option "Hello"/"Hard"/"Easy": 5 cells; arrows 8 + gap 4 each side; padding 6+6
    CHECK(ui.rect(c).w == 8 + 4 + 40 + 4 + 8 + 12);
}
