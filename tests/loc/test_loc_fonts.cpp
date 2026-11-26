// Fonts per language (spec 029 FR-29): use_fonts() loads the current language's role fonts (a role it lacks from its
// chain), swaps them at every switch (a font both languages use stays loaded), an unknown role is the built-in font;
// loc::apply() gives spec 028's widgets the resolver and the role fonts.

#include <string>

#include "loc_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/font.hpp"
#include "nnh/localization.hpp"
#include "nnh/memory.hpp"
#include "nnh/ui.hpp"
#include "test.hpp"

using namespace loc_test;
using namespace font_test;
namespace fake = nnh::platform_fake;

namespace {

FontBuilder with_line(int line) {
    FontBuilder b = FontBuilder::latin();
    b.line_height = line;
    return b;
}

/// en: body 20, title 30; ja: body 24 (its title from en); ru: nothing of its own.
void started(const char* language) {
    fresh_engine();
    LocBuilder b = four_languages();
    b.languages[0].font("body", "fonts/latin").font("title", "fonts/title");
    b.languages[3].font("body", "fonts/jp");
    for (const TestLanguage& l : b.languages) add_table_asset(l, keep(write_table(l)));
    test_assets().push_back({".nnh_loc", &keep(b.manifest())});
    test_assets().push_back({"fonts/latin.nnhfont", &keep(with_line(20).build())});
    test_assets().push_back({"fonts/title.nnhfont", &keep(with_line(30).build())});
    test_assets().push_back({"fonts/jp.nnhfont", &keep(with_line(24).build())});
    remount();
    fake::set_system_language("en");
    CHECK(nnh::loc::start({.language = language}).ok());
}

int line(nnh::loc::Key role) { return nnh::gfx::font_metrics(nnh::loc::font(role)).line_height; }

} // namespace

NNH_TEST(loc_fonts_follow_the_language_and_its_chain) {
    const StopAtEnd stop_at_end;
    started("ja");
    nnh::loc::use_fonts();
    CHECK(line("body") == 24);
    CHECK(line("title") == 30); // ja has none: en's
    CHECK(nnh::loc::set_language("ru").ok());
    CHECK(nnh::frame());
    CHECK(line("body") == 20);
    CHECK(line("title") == 30);
}

NNH_TEST(loc_fonts_are_loaded_by_font_alone_and_swapped_without_leaks) {
    const StopAtEnd stop_at_end;
    started("en");
    const int live = nnh::mem::live_allocations();
    CHECK(line("body") == 20); // font() starts using fonts
    const int with_fonts = nnh::mem::live_allocations();
    CHECK(with_fonts > live);
    for (int i = 0; i < 6; ++i) {
        CHECK(nnh::loc::set_language(i % 2 == 0 ? "ja" : "en").ok());
        CHECK(nnh::frame());
    }
    CHECK(nnh::frame()); // the old pages' textures go at the end of the frame after the switch (D47)
    CHECK(nnh::mem::live_allocations() == with_fonts);
    nnh::loc::stop();
    CHECK(nnh::frame());
    CHECK(nnh::mem::live_allocations() <= live);
}

NNH_TEST(loc_fonts_an_unknown_role_is_the_builtin_font) {
    const StopAtEnd stop_at_end;
    started("en");
    LogLines warnings(nnh::LogLevel::Warn);
    CHECK(nnh::loc::font("nope").builtin());
    CHECK(nnh::loc::font("nope").builtin());
    CHECK(warnings.count("font role \"nope\" is not declared") == (nnh::build::kDevToolsEnabled ? 1 : 0));
}

NNH_TEST(loc_fonts_a_missing_font_file_is_logged_and_builtin) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    LocBuilder b = four_languages();
    b.languages[0].font("body", "fonts/absent");
    b.install();
    fake::set_system_language("en");
    CHECK(nnh::loc::start().ok());
    LogLines errors(nnh::LogLevel::Error);
    CHECK(nnh::loc::font("body").builtin());
    CHECK(errors.contains("font fonts/absent of language en"));
}

NNH_TEST(loc_apply_gives_widgets_the_language_and_its_fonts) {
    const StopAtEnd stop_at_end;
    started("pt-BR");
    const nnh::UiDesc desc;
    auto ui = nnh::Ui::create(desc);
    CHECK(ui.ok());
    if (!ui) return;
    nnh::Ui& u = ui.value();
    nnh::UiLabelDesc label;
    label.text = nnh::UiText::name("menu.new_game");
    const nnh::UiWidget w = u.label(u.root(nnh::Screen::Top), label);
    const nnh::loc::UiFontRole roles[] = {{0, "body"}};
    nnh::loc::apply(u, roles);
    CHECK(std::string(u.text(w)) == "Novo jogo");
    CHECK(nnh::loc::set_language("en").ok());
    CHECK(nnh::frame());
    nnh::loc::apply(u, roles);
    CHECK(std::string(u.text(w)) == "New game");
}
