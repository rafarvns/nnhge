# Localizing a game

From one language to several, with fonts that can draw them. Reference pages: [localization.md](../api/localization.md)
(strings, `tr`, `format`, switching) and [fonts.md](../api/fonts.md) (font assets, drawing). The example
`examples/localization` does everything below in English, Brazilian Portuguese, German, Russian and Japanese.

## 1. Move the texts into a string file

Every text the player reads gets a key. Start with the base language:

```cmake
nnh_add_game(mygame SOURCES main.cpp ASSETS assets LANGUAGES en BASE en)
```

```toml
# assets/strings/en.strings
[language]
name = "English"

[menu]
new_game = "New game"
quit = "Quit"
[hud]
coins = "Coins: {coins}"
```

```cpp
nnh::loc::start();                                           // in main, after nnh::init()
nnh::gfx::draw_text(screen, pos, nnh::loc::tr("menu.new_game"));
char line[48];
nnh::loc::format(line, sizeof line, "hud.coins", nnh::loc::arg("coins", coins));
```

UI widgets take keys too: `UiText::name("menu.new_game")`, and `nnh::loc::apply(ui, roles)` in the scene's `on_enter()`
plus a `LanguageChanged` subscription (see [localization.md](../api/localization.md#api)).

## 2. Add languages

Add the codes to `LANGUAGES` and one file per language (or a folder `strings/<code>/` of files). A language may leave
keys out: they come from its `fallback` (default the base) and the build says how many. Typos, placeholders that don't
match the base's and missing plural forms stop the build with `file:line: reason`.

Plurals follow CLDR: English needs `one` and `other`, Russian `one`, `few`, `many`, `other`, Japanese only `other` —
the build tells you which forms a language needs. Numbers in `format` are grouped as the language's `grouping`
says (`1,234` in English, `1.234` in German, `1 234` in Russian from five digits).

## 3. Give each language fonts that can draw it

Describe fonts in `assets/fonts/*.font` ([fonts.md](../api/fonts.md#font-assets)) and name them per language as roles:

```toml
[language.fonts]        # in en.strings
body = "fonts/body_16"
title = "fonts/title_32"
```

`strings = ["en", "pt-BR"]` in a description bakes exactly the characters those languages use, so the font stays small.
A language without its own role takes the role from its fallback chain. Call `loc::use_fonts()` before `start()` and
draw with `loc::font("body")`.

**CJK on the Old 3DS**: bake the kana, the characters your strings use and, if the game shows player-entered or
data-driven text, a named set such as `kyoiku` (1 006 kanji); one 13 px page of 1 024 × 512 holds ≈ 1 400 glyphs
(295 KB). The build checks that every character of every string is in the language's first font role or its fallbacks.

**Licences**: a font made from a TrueType/OpenType file needs `licence = "<asset path>"` (packed with the game; the
example's Noto Sans JP subset ships `licences/OFL-NNHExampleSansJP.txt`) or `"own"`. Under the SIL Open Font License a
modified (subset) font is renamed; keep the script that made it ([the example's](../../examples/localization/fonts/README.md)).

## 4. Let the player choose

List `loc::language_count()` languages by `loc::language_name(i)` (each in itself — give the body font the characters
of every name, or a fallback that has them), call `loc::set_language(loc::language_code(i))` (it takes effect at the
next frame), and save the code with [saves](../api/saves.md) to pass as `start({.language = saved})` next time. Without
a saved choice the game starts in the system language, or the base. `follow_system(true)` keeps following it.

Images with text: pick the asset by `loc::code()` (`"art/title_" + code`).

## 5. Test

- `NNH_SIM_LANGUAGE=ja` starts the simulator in Japanese; test scripts switch with `language ja` and `language off`
  ([testing on the console](testing-on-the-console.md)).
- Debug builds ship the pseudo-language `qps`: every text accented, 30 % longer and bracketed — untranslated texts
  (no brackets) and clipped widgets show at once. `loc::set_show_keys(true)` (or `NNH_LOC_SHOW_KEYS=1`) shows key names.
- Hand translators the string files: they are plain UTF-8 TOML, one per language; the build's messages point at the
  exact line.
