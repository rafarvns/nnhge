// localization: game fonts and languages (spec 029). A title menu, an options screen listing every shipped language by
// its own name, a typewriter dialog, a shop with plural forms and grouped numbers and a font gallery, in English,
// Brazilian Portuguese, German, Russian and Japanese (and, in Debug/Development, the pseudo-language qps); the texts are
// keys in strings/<code>.strings, converted by the build, and every language brings its own fonts (roles body, title,
// hud). It starts in the system language and follows it; SELECT cycles the languages, Y runs the self-check (AC-2),
// X the bench (AC-6), START quits.
//
// LOC_DEMO picks the first scene: showcase (default), check, bench. LOC_SHOT sets up a screenshot:
//   LOC_SHOT=shop NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/loc.png build/pc-debug/examples/localization/localization.exe
//   (title | options | dialog | shop | gallery | ja | ru | qps | pages); NNH_SIM_LANGUAGE=ja starts in Japanese.
// Console test (spec 013): nnh run localization --script examples/localization/tests/check.nnhs

#include <cstdlib>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

#include "bench_scene.hpp"
#include "check_scene.hpp"
#include "hud.hpp"
#include "scenes.hpp"
#include "settings.hpp"

namespace {

/// The scenes and the HUD, on the heap (the 3DS main-thread stack is small).
struct Game {
    Demo demo;
    Hud hud;
    TitleScene title{demo};
    OptionsScene options{demo};
    DialogScene dialog{demo};
    ShopScene shop{demo};
    GalleryScene gallery{demo};
    CheckScene check{demo};
    BenchScene bench{demo};

    bool create() {
        demo = {&title, &options, &dialog, &shop, &gallery, &check, &bench, &hud, 0, demo.following};
        return title.create() && options.create() && dialog.create() && shop.create() && gallery.create();
    }
    nnh::Scene& first() {
        if (demo_is("check")) return check;
        if (demo_is("bench")) return bench;
        if (shot_is("options")) return options;
        if (shot_is("dialog") || shot_is("ja") || shot_is("ru") || shot_is("qps")) return dialog;
        if (shot_is("shop")) return shop;
        if (shot_is("gallery") || shot_is("pages")) return gallery;
        return title;
    }
};

const char* shot_language() {
    if (shot_is("ja")) return "ja";
    if (shot_is("ru")) return "ru";
    if (shot_is("qps")) return "qps";
    return nullptr;
}

} // namespace

int main() {
    // menus, the HUD, 400 bench glyphs and 300 sprites: up to 4096 draws; fonts' pages are textures
    if (!nnh::init({.title = "NNHGE Localization", .gfx = {.max_draws = 4096, .max_textures = 64}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(18, 20, 40));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(12, 14, 28));
    nnh::loc::use_fonts(); // the current language's role fonts, now and at every switch
    // the player's saved choice (spec 027 saves), else the system language; screenshots and demos ignore the save
    char saved[16] = {};
    const bool scripted = std::getenv("LOC_SHOT") != nullptr || std::getenv("LOC_DEMO") != nullptr;
    if (!scripted) load_language_choice(saved, sizeof saved);
    const char* language = shot_language() != nullptr ? shot_language() : (saved[0] != 0 ? saved : nullptr);
    auto started = nnh::loc::start({.language = language, .follow_system = language == nullptr});
    if (!started) {
        NNH_LOG_ERROR(example, "loc: no languages: %s", started.error().message);
    } else {
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr) game->demo.following = language == nullptr;
        if (game != nullptr && game->create()) nnh::run(game->first()); // until Quit or START
    } // the scenes and their UIs go before localization and the engine
    nnh::loc::stop();
    nnh::shutdown();
    return 0;
}
