#pragma once
// The UI example's shared state (spec 028 AC-3): the scenes, the art, the two themes (SELECT switches), the language
// (the options screen switches it), the HUD and the background. Scenes reach each other through it.

#include <nnh/log.hpp>
#include <nnh/scene.hpp>
#include <nnh/texture.hpp>
#include <nnh/ui.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

class Hud;

struct Demo {
    nnh::Scene* title = nullptr;
    nnh::Scene* options = nullptr;
    nnh::Scene* inventory = nullptr;
    nnh::Scene* gallery = nullptr;
    nnh::Scene* dialog = nullptr;
    nnh::Scene* check = nullptr;
    nnh::Scene* bench = nullptr;
    const nnh::Texture* art = nullptr;
    Hud* hud = nullptr;
    int theme = 0;    ///< 0: the 9-slice theme, 1: ui::default_theme()
    int language = 0; ///< 0: English, 1: Português (texts.cpp)
    int frames = 0;   ///< display frames since the start (the HUD's clock)
};

/// UI_DEMO=<name>: showcase (default) | check | bench.
bool demo_is(const char* name);
/// UI_SHOT (simulator screenshots): title | options | list | grid | dialog | modal | theme2, or "".
const char* shot_name();
bool shot_is(const char* name);

/// The theme SELECT chose (themes.cpp), and the art set on a UI's texture slot 0.
const nnh::UiTheme& current_theme(const Demo& demo);
void apply_look(nnh::Ui& ui, const Demo& demo);
