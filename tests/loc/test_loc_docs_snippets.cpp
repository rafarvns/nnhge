// docs/api/localization.md's examples compile and do what the page says (spec 029 T6.7): start, tr, format with a
// plural, a switch at the next frame, and the scene pattern that re-applies a UI on LanguageChanged.

#include <cstring>
#include <string>

#include "loc_fixture.hpp"
#include "nnh/events.hpp"
#include "nnh/gfx.hpp"
#include "nnh/localization.hpp"
#include "nnh/scene.hpp"
#include "nnh/ui.hpp"
#include "test.hpp"

using namespace loc_test;

namespace {

struct Settings {
    const char* language = nullptr;
};

/// The page's scene: apply in on_enter() and from a LanguageChanged subscription the scene owns.
class Title final : public nnh::Scene {
public:
    static constexpr nnh::loc::UiFontRole kRoles[] = {{0, "body"}, {1, "title"}};
    nnh::Ui ui_;
    nnh::UiWidget label_{};
    nnh::Subscription language_;
    void on_enter() override {
        nnh::loc::apply(ui_, kRoles);
        language_ = nnh::events::subscribe<&Title::on_language>(this);
    }
    void on_language(const nnh::LanguageChanged&) { nnh::loc::apply(ui_, kRoles); }
};

} // namespace

NNH_TEST(loc_docs_the_api_page_example_runs) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    four_languages().install();
    nnh::platform_fake::set_system_language("en");
    Settings settings{"pt-BR"};
    // the page's example, as written
    CHECK(static_cast<bool>(nnh::loc::start({.language = settings.language})));
    CHECK(nnh::frame());
    nnh::gfx::draw_text(nnh::Screen::Top, {8, 8}, nnh::loc::tr("menu.new_game"), {.font = nnh::loc::font("body")});
    char line[64];
    const int coins = 21;
    nnh::loc::format(line, sizeof line, "items.count", nnh::loc::arg("count", coins));
    CHECK(std::string(line) == "21 itens");
    (void)nnh::loc::set_language("ja");
    CHECK(std::string(nnh::loc::code()) == "pt-BR"); // at the next frame
    CHECK(nnh::frame());
    CHECK(std::string(nnh::loc::code()) == "ja");
}

NNH_TEST(loc_docs_the_scene_pattern_follows_the_language) {
    const StopAtEnd stop_at_end;
    fresh_engine();
    four_languages().install();
    nnh::platform_fake::set_system_language("en");
    CHECK(nnh::loc::start().ok());
    Title title;
    auto ui = nnh::Ui::create(nnh::UiDesc{});
    CHECK(ui.ok());
    if (!ui) return;
    title.ui_ = static_cast<nnh::Ui&&>(ui.value());
    nnh::UiLabelDesc label;
    label.text = nnh::UiText::name("menu.new_game");
    title.label_ = title.ui_.label(title.ui_.root(nnh::Screen::Top), label);
    title.on_enter();
    CHECK(std::string(title.ui_.text(title.label_)) == "New game");
    CHECK(nnh::loc::set_language("ru").ok());
    CHECK(nnh::frame());
    CHECK(std::string(title.ui_.text(title.label_)) == "Новая игра");
}
