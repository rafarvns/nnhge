#pragma once
// What the localization example's scenes share (spec 029): the scenes, the HUD, the UI's font roles (slot 0 the body
// font, slot 1 the title font of the current language), a theme whose styles use them, the screenshot and demo switches.

#include <nnh/localization.hpp>
#include <nnh/log.hpp>
#include <nnh/scene.hpp>
#include <nnh/ui.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

class Hud;

struct Demo {
    nnh::Scene* title = nullptr;
    nnh::Scene* options = nullptr;
    nnh::Scene* dialog = nullptr;
    nnh::Scene* shop = nullptr;
    nnh::Scene* gallery = nullptr;
    nnh::Scene* check = nullptr;
    nnh::Scene* bench = nullptr;
    Hud* hud = nullptr;
    int frames = 0;
    bool following = true; ///< following the system language (Options shows "System language")
};

/// The UI's font slots and the roles they show (loc::apply).
inline constexpr nnh::loc::UiFontRole kRoles[] = {{0, "body"}, {1, "title"}};

/// The default theme with every text in the body font (slot 0).
const nnh::UiTheme& example_theme();
/// A label style in the title font (slot 1), with an outline-like shadow.
nnh::UiStyle title_style();

bool demo_is(const char* name); ///< LOC_DEMO: showcase (default), check, bench
bool shot_is(const char* name); ///< LOC_SHOT: title, options, dialog, shop, gallery, ja, ru, qps, pages

/// SELECT: the next shipped language (the pseudo-language skipped), at the next frame.
void next_language();
