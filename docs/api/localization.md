# localization — text in the game's languages

Header: `<nnh/localization.hpp>` · Spec: [029](../../specs/029-game-fonts-and-localization/spec.md) ·
Decision: [D50](../architecture/decisions.md) · Example: `examples/localization` · Fonts: [fonts.md](fonts.md) ·
Guide: [localizing a game](../guides/localizing-a-game.md)

The game's texts live in one string file per language, checked and converted by the build; at run time `tr("key")`
returns the current language's text and `format()` fills placeholders and picks plural forms — no printf, no
allocation. One language is current for the whole game; it is chosen at start (the player's saved choice, else the
system language, else the base) and switched at the start of the next frame. Optional: a game that never calls `loc::`
links none of it.

```cmake
nnh_add_game(mygame SOURCES main.cpp ASSETS assets LANGUAGES en pt-BR ja BASE en)
```

```toml
# assets/strings/en.strings (or every *.strings of assets/strings/en/)
[language]
name = "English"
[language.fonts]
body = "fonts/body_16"

[menu]
new_game = "New game"
[shop]
items.one = "{count} lantern"
items.other = "{count} lanterns"
greeting = "Welcome, {name}!"
```

```cpp
if (!nnh::loc::start({.language = settings.language})) return false;  // NotFound: no LANGUAGES
nnh::gfx::draw_text(nnh::Screen::Top, {8, 8}, nnh::loc::tr("menu.new_game"), {.font = nnh::loc::font("body")});
char line[64];
nnh::loc::format(line, sizeof line, "shop.items", nnh::loc::arg("count", coins));      // "21 lanterns"
(void)nnh::loc::set_language("ja");                                                    // at the next frame
```

## String files

- Keys: parts of `[A-Za-z0-9_-]` joined by dots (≤ 128 bytes); a `[section]` is a prefix. Values are `"basic"` or
  `"""multi-line"""` strings (UTF-8, ≤ 4 096 bytes; a line ending in `\` joins the next without its indentation).
  `# comments`. A file ≤ 1 MiB.
- `[language]`: `name` (shown in language menus, in the language itself), `fallback` (another shipped code; default the
  base), `grouping` (the digit group separator, e.g. `","`, `"."`, `" "`; `""` for none), `grouping_min` (digits
  from which numbers are grouped, default 4), `[language.fonts]` (role = font asset path, ≤ 8 roles),
  `[language.plural]` (CLDR rules for a language the engine doesn't know), `[language.limits]` (base only: key prefix =
  maximum characters, a warning above).
- Placeholders: `{name}`; `{{` and `}}` are literal braces.
- Plurals: `key.zero/one/two/few/many/other`, the argument `count` (or `key.plural = "name"`). The build knows CLDR 46's
  integer rules for `ja zh ko vi id ms th` (other), `en de nl sv da nb fi et it el hu tr bg ca` (one), `es fr pt` (one,
  many), `pt-PT`, `ro`, `ru uk`, `pl`, `cs sk`, `hr sr`, `sl`, `lt`, `lv`, and requires exactly the forms a language's
  rule produces.
- Codes: `en`, `pt-BR`, `zh-Hant`, `es-419` (well-formed tags); the base is `BASE` (default the first of `LANGUAGES`).

The build stops on errors and prints warnings as `file:line: reason` — e.g. `strings/de.strings:20: "menu.optons" is
not in the base language en (did you mean "menu.options"?)`, a placeholder missing or unknown, a plural form missing for
the language's rule, a character the language's first font role can't draw (`… uses U+8CB7 '買' not in font
fonts/body_ja_13 (role body) or its fallbacks`), keys the language hasn't translated (one warning: the base's text is
used). `STRICT_STRINGS` turns warnings into errors. Debug/Development builds also make the pseudo-language `qps`
(accented, 30 % longer, in brackets) to find untranslated and clipped text; Release builds keep no key names.

## API

| API | |
|-----|-|
| `loc::start({.language, .follow_system, .tag})` | reads `.nnh_loc`, allocates one block for the largest language chain, loads the starting language, publishes `LanguageChanged`. Errors: `NotFound`, `Corrupted`, `OutOfMemory` |
| `loc::stop()` | frees everything (`nnh::shutdown()` does it in Debug/Development; a Release game that restarts the engine calls it first) |
| `loc::tr(key)` | the current language's text, else its fallback chain's, else the base's; never null; valid until the next switch takes effect |
| `loc::format(out, capacity, key, args...)` | placeholders replaced, the plural form chosen, truncated at a UTF-8 boundary; returns the bytes written |
| `loc::arg(name, integer)`, `arg(name, "text")`, `arg_key(name, key)` | format's arguments (integers grouped as the language says) |
| `loc::set_language(code)` / `follow_system(on)` | switch at the next frame (the last call wins; `NotFound` for a code not shipped) / follow `system::language()` |
| `loc::code()`, `name()`, `index()`, `serial()`, `language_count()`, `language_code(i)`, `language_name(i)`, `find(code)` | queries |
| `loc::key(name)` | a key from a run-time name (data-driven text); literal keys hash at compile time |
| `loc::resolve(hash)` | `tr` by hash: `ui.set_resolver(&nnh::loc::resolve)` |
| `loc::use_fonts()`, `loc::font(role)` | the current language's role fonts, loaded at start and at every switch (the first `font()` turns `use_fonts()` on if the game hasn't) |
| `loc::apply(ui, roles)` | spec 028's widgets in the current language: the resolver, each slot's font, `text_changed()` |
| `loc::dump()`, `loc::set_show_keys(on)` | Debug/Development: the current language; key names instead of texts (also `NNH_LOC_SHOW_KEYS=1`) |

A key no table has shows its name in Debug/Development (logged once) and `??` in Release. `LanguageChanged {previous,
current, serial}` is published when a switch took effect, in the frame it did; widgets re-apply from a subscription the
scene owns:

```cpp
void on_enter() override {
    nnh::loc::apply(ui_, kRoles);                                   // kRoles = {{0, "body"}, {1, "title"}}
    language_ = nnh::events::subscribe<&Title::on_language>(this);
}
void on_language(const nnh::LanguageChanged&) { nnh::loc::apply(ui_, kRoles); }
```

The system language comes from `system::language()` ([system.md](system.md)): the 3DS's System Settings, the
simulator's `NNH_SIM_LANGUAGE` or `language=` in `language.ini` beside its `window.ini` ([simulator.md](simulator.md)),
and a test script's `language` command ([testing on the console](../guides/testing-on-the-console.md)).

## Costs and memory

`tr` is a bucket and one or two compares per table of the chain; `format` copies spans and writes digits with constant
divisions (no `__aeabi_*` calls on the ARM11, no float). A switch reads the new chain's tables into the same block and
loads the new role fonts before releasing the old ones. Nothing allocates after `start()` but the role fonts at a switch.
A table costs 12 bytes per key, 8 per string and the text, plus 4 bytes and the name per key in Debug/Development; the
example's English table (27 keys) takes 1.3 KB in Release. On the PC simulator (Development build): 1 000 `tr` 0.042 ms, 100 `format` 0.018 ms. Console numbers come
with spec 029's console run.

## Misuse

`tr`/`set_language` before `start()` assert (`tr` returns `""` in Release); `start()` twice asserts and changes nothing;
`format` without a buffer asserts; a placeholder without an argument shows `{name}` in Debug/Development (logged) and
nothing in Release.
