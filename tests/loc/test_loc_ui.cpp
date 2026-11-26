// Spec 028's widgets in the current language (spec 029 FR-30): loc::apply() from a LanguageChanged subscription shows
// the new texts and role fonts in the frame the switch took effect; keys meet through the same hash as UiText::name.

#include <string>

#include "loc_fixture.hpp"
#include "nnh/events.hpp"
#include "nnh/localization.hpp"
#include "nnh/ui.hpp"
#include "test.hpp"

using namespace loc_test;
using namespace font_test;

namespace {

nnh::Ui* g_ui = nullptr;
const nnh::loc::UiFontRole kRoles[] = {{0, "body"}};

void reapply(const nnh::LanguageChanged&) { nnh::loc::apply(*g_ui, kRoles); }

} // namespace

NNH_TEST(loc_ui_keys_hash_like_ui_text_names) {
    const StopAtEnd stop_at_end;
    CHECK(nnh::loc::Key("menu.new_game").hash == nnh::UiText::name("menu.new_game").key_value());
    CHECK(nnh::loc::key("a.b.c").hash == nnh::UiText::name("a.b.c").key_value());
}

NNH_TEST(loc_ui_a_subscription_shows_the_new_language_in_the_same_frame) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    LocBuilder b = four_languages();
    b.languages[0].font("body", "fonts/latin");
    b.languages[3].font("body", "fonts/jp");
    for (const TestLanguage& l : b.languages) add_table_asset(l, keep(write_table(l)));
    test_assets().push_back({".nnh_loc", &keep(b.manifest())});
    FontBuilder wide = FontBuilder::latin();
    wide.line_height = 24;
    test_assets().push_back({"fonts/latin.nnhfont", &keep(FontBuilder::latin().build())});
    test_assets().push_back({"fonts/jp.nnhfont", &keep(wide.build())});
    remount();
    nnh::platform_fake::set_system_language("en");
    CHECK(nnh::loc::start().ok());
    const nnh::UiDesc desc;
    auto ui = nnh::Ui::create(desc);
    CHECK(ui.ok());
    if (!ui) return;
    g_ui = &ui.value();
    nnh::UiLabelDesc label;
    label.text = nnh::UiText::name("menu.new_game");
    const nnh::UiWidget w = g_ui->label(g_ui->root(nnh::Screen::Top), label);
    auto sub = nnh::events::subscribe<nnh::LanguageChanged>(&reapply);
    CHECK(nnh::frame()); // start's event: applied
    CHECK(std::string(g_ui->text(w)) == "New game");
    CHECK(nnh::loc::set_language("ja").ok());
    CHECK(nnh::frame());
    CHECK(std::string(g_ui->text(w)) == "ニューゲーム");
    g_ui = nullptr;
}
