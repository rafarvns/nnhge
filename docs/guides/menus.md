# Menus with UI widgets

How to build the usual screens of a game with [`<nnh/ui.hpp>`](../api/ui.md): a title menu, an options screen, an
inventory, a dialog, a prompt, a HUD, tabs, animation, sounds, and a theme kept in a data file. Every snippet comes
from (or is checked against) [`examples/ui`](../../examples/ui/main.cpp); the API page has the rules.

## The pattern

One `nnh::Ui` per screen, as a **member of its scene**, built once and **bound** to the scene. The binding updates the
UI right before the scene's `update()`, so `update()` reads this frame's results; it freezes the UI under a pushed
scene and remembers the focus when you come back, with no code.

```cpp
class TitleScene : public nnh::Scene {
public:
    bool create() {
        auto made = nnh::Ui::create({.widgets = 40, .text_bytes = 64});
        if (!made) return false;
        ui_ = std::move(made.value());
        menu_ = ui_.vbox(ui_.root(nnh::Screen::Bottom),
                         {.spacing = 3, .layout = {.width = 160, .place = {.anchor = nnh::UiAnchor::Center}}});
        ui_.button(menu_, {.text = "New game", .key = Key::New});
        ui_.button(menu_, {.text = "Options", .key = Key::Options});
        ui_.button(menu_, {.text = "Quit", .key = Key::Quit});
        ui_.bind(*this);
        return true;
    }
    void update() override {
        if (ui_.activated(Key::Options)) nnh::scenes::push(options_);
    }
    void draw() override { ui_.draw(); }
    // ...
};
```

Keys are your own enum: switch on `Key::Options` instead of comparing handles, and restore the focus of a rebuilt
menu with `ui_.focus(saved_key)`. Size the UI like `AppConfig::gfx.max_draws`: a full UI returns null handles and
asserts in Debug.

## Title menu

A centred VBox of buttons on the bottom screen and the game's name on the top one
([`title_scene.cpp`](../../examples/ui/title_scene.cpp)). A disabled entry is skipped by the D-Pad
(`set_enabled(cont, false)`: "Continue" without a save). A locked entry that should explain itself goes in a box with
`.disabled_focusable = true`: it takes the focus but activating it reports `Refused` (play a buzz, show "Locked").
The first update focuses the bottom root's first target (`UiDesc::first_focus`, `set_default`). The A that chose
"Options" won't press the options screen's first button: a UI ignores every control held when it becomes active.

## Options screen

A panel filling the bottom screen holds a `scroll_box` (`.layout = {.expand = true}`) of sliders, toggles and choices
([`options_scene.cpp`](../../examples/ui/options_scene.cpp)): Left/Right change values with repeat, Up/Down move,
touches drag sliders and tap toggles and choice arrows, the box scrolls by whole rows to keep the focus visible. "Back"
is the box's back widget (`ui_.set_back(box, back)`): B activates it, so `if (ui_.activated(Key::Back)) pop();` handles
both. Read values with `ui_.value(Key::Music)` or react in the frame they change (`ui_.changed(Key::Music)`).

## A language option

A `choice` lists "System language" and then every shipped language by its own name (`nnh::loc::language_name(i)`, kept
alive by localization, passed as `UiText::borrow`); changing it calls `nnh::loc::set_language(code)` (or
`follow_system(true)`) and saves the code with [saves](../api/saves.md). The scene calls `nnh::loc::apply(ui_, roles)`
in `on_enter()` and from a `LanguageChanged` subscription, so every widget shows the new language and fonts in the
frame the switch takes effect. Give the body font the characters of every language's name (or a fallback font that has
them). Example: [`examples/localization/options_scene.cpp`](../../examples/localization/options_scene.cpp); guide
[localizing a game](localizing-a-game.md).

## Inventory

A `list` with a row fill function and the item's description elsewhere
([`inventory_scene.cpp`](../../examples/ui/inventory_scene.cpp)):

```cpp
void fill_row(int index, nnh::UiRow& row) { // only for rows coming into view
    row.text = item_name(index);
    row.number = item_count(index);
    row.number_prefix = "x";
    row.icon_texture = 0;
    row.icon = item_icon(index);
}
list_ = ui_.list(root, {.rows = 13, .count = kItems, .fill = &fill_row, .margin = 2, .cell_width = 280});
// update(): the focused item's description
for (const nnh::UiResult& r : ui_.results())
    if (r.kind == nnh::UiResultKind::FocusChanged && r.widget == list_) describe(r.index);
```

L/R page. After the inventory changes, call `set_item_count` or `mark_items_changed(list, first, count)`. A grid is
the same call with `.columns > 1` ([`gallery_scene.cpp`](../../examples/ui/gallery_scene.cpp)).

## Dialog box

A panel with a wrapping label revealed a character per frame
([`dialog_scene.cpp`](../../examples/ui/dialog_scene.cpp)): `.wrap = true, .reveal_rate = 1.0f`; A calls
`show_all(label)` when `!fully_shown(label)`, else closes the dialog. The text is wrapped as a whole, so words never jump
lines while appearing.

## Prompt

```cpp
prompt_ = ui_.prompt(nnh::Screen::Bottom, {.message = "Quit the game?", .buttons = {"Yes", "No"},
                                           .keys = {Key::Yes, Key::No}, .default_button = 1, .cancel_button = 1});
// later: if (ui_.activated(Key::Yes)) nnh::scenes::pop();
```

The prompt is modal (only its buttons react; B answers No) and closes itself on the answer; the focus goes back where
it was.

## HUD

Its own UI with only a top root on layer 13, never updated (a UI without input still draws): a progress bar anchored
top-left and a label anchored top-right ([`hud.cpp`](../../examples/ui/hud.cpp)). Change it with `set_value` and, for a
borrowed text buffer, `mark_text_changed`. `ignore_camera` is always on: the camera never moves widgets.

## Animating with tweens

The UI never includes the tween header; a tween drives a widget through a setter. `set_render_offset` and `set_opacity`
move and fade a widget and its subtree when drawn, without relaying out:

```cpp
tweens_.from_to(nnh::Setter<float>{[this](float x) { ui_.set_render_offset(panel_, {x, 0}); }}, 320.0f, 0.0f,
                nnh::Frames{20}, {.ease = nnh::Ease::CubicOut});
```

## Sounds from results

Play sounds where you read results, in the same frame:

```cpp
for (const nnh::UiResult& r : ui_.results()) {
    switch (r.kind) {
    case nnh::UiResultKind::FocusChanged: nnh::audio::play(tick_); break;
    case nnh::UiResultKind::Activated: nnh::audio::play(click_); break;
    case nnh::UiResultKind::Refused:
    case nnh::UiResultKind::Blocked: nnh::audio::play(buzz_); break;
    default: break;
    }
}
```

An audio scene that doesn't own the UI can listen to the bus instead after `ui_.set_events(true)` (`UiActivated`,
`UiFocusChanged`, ...; delivered at the next frame).

## Tabs

No tab container is needed: one panel per tab, all but one hidden, switched by the page results.

```cpp
for (const nnh::UiResult& r : ui_.results()) {
    if (r.kind != nnh::UiResultKind::Page) continue;
    tab_ = (tab_ + r.value + kTabs) % kTabs;
    for (int i = 0; i < kTabs; ++i) ui_.set_visible(tabs_[i], i == tab_);
    ui_.focus(tabs_[tab_]); // its remembered child
}
```

## Remapped controls

With spec 017 actions, `ui_.use_actions(actions_, {.up = Act::Up, .down = Act::Down, .left = Act::Left,
.right = Act::Right, .confirm = Act::Confirm, .cancel = Act::Back})` makes the menus follow the player's bindings
(`move` reads a vector action as four directions). Linked only by games that call it.

## A theme in a data file

The style structs are plain data kinds, so a theme can live in a [data file](../api/data.md): declare the style structs
and your own theme type in your schema source, load it and convert it to a `UiTheme` you keep alive:

```cpp
// data_types.cpp (your schema source)
NNH_DATA_ENUM(nnh::UiBackgroundKind, None, Solid, NineSlice);
NNH_DATA_STRUCT(nnh::UiBackground, NNH_DATA_FIELD(kind), NNH_DATA_FIELD(texture), NNH_DATA_FIELD(color),
                NNH_DATA_FIELD(region), NNH_DATA_FIELD(left), NNH_DATA_FIELD(top), NNH_DATA_FIELD(right),
                NNH_DATA_FIELD(bottom));
// ... UiLook, UiTextLook, UiStyle and the rest as in tests/ui/data_check/style_types.cpp
NNH_DATA_TYPE(GameTheme, "theme", NNH_DATA_FIELD(button), NNH_DATA_FIELD(panel));
```

`tests/ui/data_check/style_types.cpp` declares every style struct this way (the ctest
`ui_style_structs_are_data_loadable` compiles it). Without a theme file, a game styles its UI in C++ like
[`themes.cpp`](../../examples/ui/themes.cpp): copy `nnh::ui::default_theme()` and change what you need.
