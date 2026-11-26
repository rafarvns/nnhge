// ui: widgets (spec 028). A title menu, an options screen in a scroll box (slid in by a tween), a 300-item inventory
// with its description on the top screen, a grid gallery, a typewriter dialog, a "Quit?" prompt and a HUD over a
// scrolling background, in a 9-slice theme or the engine's default one (SELECT). Every result is logged as one line
// ("ui: focus title.options", "ui: value music 3", "ui: refused online", ...). Y the self-check (AC-2), X the bench
// (AC-6), START quits.
//
// UI_DEMO picks the first scene: showcase (default), check, bench. UI_SHOT sets up a screenshot:
//   UI_SHOT=options NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/ui.png build/pc-debug/examples/ui/ui.exe
//   (title | options | list | grid | dialog | modal | theme2)
// Console test (spec 013): nnh run ui --script examples/ui/tests/check.nnhs

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <nnh/app.hpp>
#include <nnh/gfx.hpp>

#include "bench_scene.hpp"
#include "check_scene.hpp"
#include "dialog_scene.hpp"
#include "gallery_scene.hpp"
#include "hud.hpp"
#include "inventory_scene.hpp"
#include "options_scene.hpp"
#include "title_scene.hpp"

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("UI_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

const char* shot_name() {
    const char* shot = std::getenv("UI_SHOT");
    return shot != nullptr ? shot : "";
}

bool shot_is(const char* name) { return std::strcmp(shot_name(), name) == 0; }

namespace {

/// The scenes and the HUD, on the heap (the 3DS main-thread stack is small).
struct Game {
    Demo demo;
    Hud hud;
    TitleScene title{demo};
    OptionsScene options{demo};
    InventoryScene inventory{demo};
    GalleryScene gallery{demo};
    DialogScene dialog{demo};
    CheckScene check{demo};
    BenchScene bench{demo};

    bool create(const nnh::Texture* art) {
        demo.art = art;
        demo.hud = &hud;
        demo.title = &title;
        demo.options = &options;
        demo.inventory = &inventory;
        demo.gallery = &gallery;
        demo.dialog = &dialog;
        demo.check = &check;
        demo.bench = &bench;
        if (shot_is("theme2")) demo.theme = 1;
        return hud.create(demo) && title.create() && options.create() && inventory.create() && gallery.create() &&
               dialog.create();
    }
    nnh::Scene& first() {
        if (demo_is("check")) return check;
        if (demo_is("bench")) return bench;
        if (shot_is("options")) return options;
        if (shot_is("list")) return inventory;
        if (shot_is("grid")) return gallery;
        if (shot_is("dialog")) return dialog;
        return title;
    }
};

} // namespace

int main() {
    // menus, the HUD, a 300-sprite bench: up to 4096 draws a frame
    if (!nnh::init({.title = "NNHGE UI", .gfx = {.max_draws = 4096}})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(14, 18, 36));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(10, 12, 22));
    {
        auto art = nnh::load_texture("art/ui");
        const nnh::Texture* sheet = art ? &art.value() : nullptr;
        if (!art) NNH_LOG_ERROR(example, "ui: no art (%s): the default theme only", art.error().message);
        std::unique_ptr<Game> game(new (std::nothrow) Game());
        if (game != nullptr && game->create(sheet)) nnh::run(game->first()); // until the title's Quit → Yes, or START
    } // the scenes, their UIs and the art go before the engine
    nnh::shutdown();
    return 0;
}
