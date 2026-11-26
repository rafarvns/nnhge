# Spec 029: Game fonts and localization

Status: implemented 2026-11-26 (Linux checks T9 passed; the console run T10.2 pending); approved 2026-11-25 (the maintainer, every open question as recommended)
Author: NNHGE maintainers · Date: 2026-11-25
Depends on: [spec 005](../005-text-and-builtin-font/spec.md) (`gfx::draw_text`/`measure_text`, the built-in font, UTF-8
decoding, the hollow box, one sprite per glyph, `max_draws`), [spec 004](../004-2d-graphics/spec.md) (textures, the draw
list, layers, `ignore_camera`, zoom through D33), [spec 007](../007-assets-and-romfs/spec.md) (the assets folder, paths,
romfs/PC packaging, build-time conversion), [spec 014](../014-data-assets/spec.md) (the TOML-subset text reader, build-time
checks with `file:line: reason`, D27's `nnh-assetc`, FNV-1a names), [spec 028](../028-ui-widgets/spec.md) (the text seam:
`UiText::key`/`UiText::name`, `set_resolver`, `text_changed()`, `UiTextLook::font`, wrapping, the ellipsis), [spec 002](../002-core-foundation/spec.md)
(`Result`, tagged memory, logs, `nnh::fs`, `nnh::system`), [spec 001](../001-event-bus-and-scenes/spec.md) (the event bus).
Works with [spec 027](../027-save-versions/spec.md) (a game saves the player's language in its settings) and
[spec 013](../013-console-remote/spec.md) (scripts choose the language). Implements two ROADMAP items: "Game fonts" (M2
follow-up) and "Localization" (M7, Features in the spirit of Godot and Unity) — the second of the last four features
before NNHGE 1.0. Optional for games (constitution XI, decision D25).

## Problem and goal

Today every NNHGE game speaks in one voice: spec 005's 8×8 monospace font, Latin-1 only. A game can't use the pixel
font its artist drew, a rounded proportional font for dialogue or a big title font; it can't show `€`, `ł`, `ş`,
Cyrillic, Greek or Japanese; and a game that wants several languages must write its own string tables, its own
"which language does the console use" code, its own placeholder formatting and plural rules, and its own resolver for
spec 028's widgets — and finds a missing translation or a missing glyph only when a hollow box shows on the console.
Godot (`tr()`, translation CSVs, `TranslationServer`, font resources) and Unity (Localization string tables with smart
strings, TextMeshPro font assets) give games **fonts as assets** and **text by key per language**. After this spec a
game puts **its own fonts** in its assets — bitmap fonts exported by common tools (BMFont format) or TrueType/OpenType
files **rasterised by the build** at the sizes it asks for — and draws with them through the same `gfx::draw_text` and
`measure_text` calls, with proportional advances, kerning, line height and baseline, **the same pixels on PC and 3DS**.
It writes **one string file per language** in its assets; the build checks every file (keys, placeholders, plural
forms, and that every character of every string exists in the fonts that language uses) and stops with
`file:line: reason`; at run time the game asks for text by key (`tr("menu.new_game")`), formats numbers and names
into placeholders with the right plural form without allocating, starts in **the console's system language** (or the
simulator's), switches language at run time, and spec 028's widgets follow the switch with one call. A game that
doesn't use any of this links nothing new.

## User stories

- As a game developer, I want to drop my pixel artist's bitmap font (exported from BMFont or a similar tool) into my
  assets and draw my HUD with it, so that my game doesn't look like every other NNHGE game.
- As a game developer, I want to use a TrueType/OpenType font I'm allowed to ship (an OFL font, my own) at 12, 16 and
  32 pixels without exporting anything by hand, so that the build makes the bitmaps and the console never rasterises.
- As a game developer, I want proportional text with kerning, a known line height and baseline, measured exactly as it
  will draw, so that boxes, centring and spec 028's wrapping are right on PC and on the console.
- As a game developer, I want a font asset to include only the characters my game uses (computed from my string files),
  so that a Japanese font fits in an Old 3DS's memory.
- As a game developer, I want the build to tell me `strings/ja.strings:41: "shop.buy" uses U+8CB7 '買' not in font
  fonts/body_ja` and `strings/de.strings:12: "items.count": placeholder {count} missing (en has it)`, so that hollow
  boxes and broken sentences never reach the console.
- As a game developer, I want to write my English texts once, hand the files to translators as plain text, and have
  every language that lacks a string fall back to English, so that a partial translation still plays.
- As a game developer, I want `tr("menu.new_game")` and `format("items.count", count = 3)` → "3 items" / "3 Gegenstände" /
  "3 предмета" with each language's plural rules, without `printf` or allocation, so that sentences are correct in every
  language I ship.
- As a game developer, I want my game to start in the language the console is set to (if I ship it), let the player
  change it in the options menu, and store that choice in my settings save, so that players get their language without
  a question at boot.
- As a game developer using spec 028's widgets, I want my menus to switch language with one call — texts re-resolved,
  fonts swapped for the new language, layout redone — so that the options screen's "Language" choice just works.
- As a game developer, I want Japanese, Chinese or Korean text to wrap correctly (no spaces between words) and not to put
  `。` or `」` at the start of a line, so that CJK dialogue looks professional.
- As a game developer testing on the console, I want a script to switch the language (`language ja`) and compare
  screenshots and logs in each language, and a pseudo-language that shows untranslated or too-long strings at a glance,
  so that localization bugs are found by tests.
- As a game developer who uses only the built-in font and English, I want my game, its size and its build time to stay
  as they are.
- As a contributor, I want the font converter, the string checker, the formatter and plural rules tested without a
  console and the converters' parsers fuzzed, because fonts and string files are external input.

## Functional requirements

### Fonts as assets

- **FR-1 Font sources.** A game adds fonts to its assets folder in either form; both are converted by the build
  (`nnh-assetc`, D27) into one NNHGE font asset per font and size, loaded by path like textures:
  - **Bitmap fonts in the BMFont format** (AngelCode's `.fnt`, its text form at least; the plan decides whether the XML
    and binary forms are accepted too) with their PNG pages beside them — the output of BMFont, Hiero, Glyph Designer,
    ShoeBox and similar tools. Glyph rectangles, offsets, advances, kerning pairs, line height and base are taken from
    the file. Pages may be greyscale/alpha (tinted by the text colour) or full colour (a pre-coloured pixel font, drawn
    with the colour as a tint).
  - **TrueType/OpenType outline fonts** (`.ttf`, `.otf` with TrueType or CFF outlines) described by a small hand-written
    **font description** text file (spec 014's TOML subset) naming the source file, one or more **pixel sizes**, the
    **character set** (FR-3), anti-aliasing on or off (off = a threshold for pixel fonts designed on a grid), padding
    and an optional baked outline (FR-9). The build rasterises every glyph once, on the host, with a vendored
    permissively licensed rasteriser (Open questions), so the console never rasterises and every system draws the same
    bitmaps.
  Source font files are build inputs, not shipped (only the converted asset goes into romfs or the PC folder). Each
  converted font names the **licence** text the game supplies (a `licence` field in the description, or a sidecar file),
  and the build copies it into the game's package so OFL-style attribution requirements are met (the plan decides where).
- **FR-2 Converted glyph pages.** The converter packs glyphs into **pages** that respect constitution II: power-of-two
  sides from 8 to 1024, a documented maximum number of pages per font (8 proposed), a documented padding between glyphs
  so no neighbour bleeds at any integer scale or zoom. Mono pages use a single-channel **alpha** format (Open questions:
  A8 or A4, a platform contract addition) so a 1024×1024 page costs 1 MiB (A8) instead of 4 MiB (RGBA); colour pages
  stay RGBA. A font that doesn't fit its limits is refused with `file: reason` (e.g. `fonts/body_ja.font: 6214 glyphs at
  16 px need 11 pages of 1024×1024 (limit 8): reduce the size or the character set`).
- **FR-3 Character sets.** A TTF/OTF description chooses its characters by any union of: named ranges (`ascii`,
  `latin1`, `latin-ext-a`, `greek`, `cyrillic`, `kana`, `cjk-punctuation`, `hangul`, `jis-level1`, … — the plan fixes the
  list), explicit code points and ranges (`U+20AC`, `U+0100..U+017F`), the characters of a text file, and **the
  characters used by the game's string files** for the languages that use this font (FR-19), so a CJK font contains only
  what the game shows. A BMFont file's characters are whatever the tool exported. The build reports each font's glyph
  count, pages and bytes.
- **FR-4 Sizes and styles.** A font asset is one face at one pixel size: bold, italic and other sizes are separate
  descriptions or separate entries of one description (one converted asset each, e.g. `fonts/body_16`, `fonts/body_bold_16`).
  Drawing scales stay **integer** (1–8, as spec 005), so baked pixels are never resampled; a game wanting a 24 px font
  bakes 24 px. Variable fonts are read at their default instance (or a named instance, if the plan allows it).
- **FR-5 Loading a font.** `load_font("fonts/body_16")` returns a `Result` with a **font handle** the game owns
  (movable, not copyable, RAII like a texture): its pages are created as textures (accounted in the `font` memory tag)
  and its glyph table read into one block. Errors (`NotFound`, `InvalidData`, `OutOfMemory`) are returned. Loading
  happens at load time, never per frame. A game may load several fonts; the plan states the per-font overhead.
- **FR-6 Metrics.** A font exposes integer **line height**, **ascent** (baseline from the top of a line), **descent**,
  and per-character queries: whether it has a glyph, its **advance**, and the **kerning** between two characters, all in
  whole pixels at scale 1 (fractional advances and kerning from an outline font are rounded once, at build time, by a
  documented rule). Spec 005's built-in font answers the same queries (advance 8, line height 10, ascent 8, no kerning).

### Drawing and measuring

- **FR-7 One drawing call for every font.** `gfx::draw_text` and `gfx::measure_text` take an optional **font** in
  their options; none means the built-in font, with output byte-identical to today. With a game font, each glyph is one
  sprite of a font page through the same recorded, layered draw list as spec 005 (colour as tint, scale, layer,
  `ignore_camera`, `Camera2D` zoom per D33, alignment of each line, `shadow`), positioned at `pen + offset`, the pen
  advancing by `advance + kerning(previous, current)`. `\n` starts a new line one line height down; `\r` is ignored;
  spaces and characters without a bitmap cost no draw. Position is the top-left of the first line's box, as today; a
  query gives the baseline offset so texts in different fonts can share a baseline.
- **FR-8 Measuring equals drawing.** `measure_text` with a font returns the width of the widest line (advances +
  kerning, the last glyph's ink included as the plan defines) and `lines × line height`, × scale, in whole pixels, and is
  exactly the box the drawn glyphs use: a test checks that no drawn pixel falls outside the measured box (save the
  documented shadow and outline extents). A **per-character measure** (pen positions of each code point of a line) is
  available for cursors and typewriter effects.
- **FR-9 Outline and shadow.** The existing `shadow` option works with every font (one more draw per glyph). A TTF/OTF
  description may bake an **outline** of 1–3 pixels: each glyph gets a second, outline glyph in the same pages, drawn
  first in an outline colour from the text options (one more draw per glyph); text without an outline colour draws no
  outline. BMFont files with an outline baked by the tool are coloured pages (FR-1). (Open questions.)
- **FR-10 Wrapping.** Text options gain an optional **wrap width**: `draw_text` and `measure_text` then break lines by
  one rule shared with spec 028's labels (one implementation): at spaces (dropped at the break), after hyphens, at `\n`,
  between any two characters of scripts written without spaces (CJK ideographs, kana; Hangul breaks at spaces), and
  inside a word longer than the width, with a small fixed **line-breaking table** (no line starts with `、。，．）」』】〕！？ー`
  and the like, none ends with `（「『【〔`; the plan names the table from JIS X 4051/UAX #14's basic classes) — the same
  breaks on every system. An optional **maximum lines** with an ellipsis (the font's `…` if it has one, else `...`)
  matches spec 028's labels.
- **FR-11 Missing characters.** A character the font lacks draws, in order: the glyph of the font's **fallback fonts**
  (a font may name up to 3 fallback fonts in its description, e.g. a Latin font falling back to a CJK font; metrics of
  each glyph come from the font that has it, the line height from the first font), else the font's own **replacement
  glyph** (U+FFFD or `?` if the font has one, chosen at build time), else spec 005's hollow box at the font's size.
  Invalid UTF-8 draws the replacement too. Nothing crashes or reads past the string. Debug/Development logs each missing
  code point once per font (`text: U+4E00 not in fonts/body_16`); Release logs nothing.
- **FR-12 Unicode scope.** Strings are UTF-8; every code point U+0000..U+10FFFF is decoded (invalid sequences, overlongs
  and surrogates are replaced). Scripts supported for drawing are those that need no shaping: Latin (all ranges),
  Greek, Cyrillic, Armenian, Georgian, Japanese, Chinese, Korean Hangul syllables, and symbols — precomposed characters,
  left to right, one glyph per code point. Combining marks, variation selectors and zero-width joiners draw nothing and
  advance nothing (the build warns when a string file contains them, FR-19). Right-to-left and shaped scripts are
  non-goals.
- **FR-13 Same pixels everywhere.** The same string, font, options and position give the same draw items on the fake
  platform, every PC host compiler and the 3DS, and the same pixels in simulator and console screenshots (alpha pages
  blended per spec 004's rules; the plan records the comparison as spec 028 AC-5 did). Measuring, wrapping and ellipsis
  use integers only.

### Fonts in spec 028's widgets

- **FR-14 Widget fonts.** A UI style's text part (`UiTextLook::font`) can name a game font: the UI holds a small table of
  **font slots** (as it holds texture slots; 8 proposed) set by the game, and the style names a slot or the built-in font.
  All widget measuring, wrapping, the ellipsis, typewriter reveal and drawing then use that font's metrics through the
  existing seam, which grows from monospace cells to per-character advances and kerning (Constraints: under pressure).
  Widgets with the built-in font lay out and draw exactly as today (spec 028's golden `30400DA6` unchanged). Rebinding a
  slot (a language switch that changes fonts) relayouts the widgets that use it.

### Localization: string files

- **FR-15 Languages of a game.** A game declares, once in its build (the plan decides: `nnh_add_game(... LANGUAGES en
  pt-BR ja BASE en ...)` or a manifest file in the assets), the **languages** it ships and its **base language** (the
  one every key must exist in, and the last fallback). Languages are named by BCP 47-style codes: a language and an
  optional region or script (`en`, `pt-BR`, `zh-Hans`). A game without the declaration and without string files links and
  converts nothing of this spec.
- **FR-16 One string file per language.** Each language's strings live in hand-written UTF-8 text files in spec 014's
  TOML subset (comments, `key = "text"`, sections as key prefixes, multi-line strings), one file per language (the plan
  decides whether a language may span several files, e.g. `strings/ja/menu.strings` and `strings/ja/dialog.strings`).
  Illustrative:

  ```toml
  # strings/en.strings
  [language]
  name = "English"               # shown in a language menu, in its own language
  fonts = { body = "fonts/body_16", title = "fonts/title_32" }

  [menu]
  new_game = "New game"
  continue = "Continue"

  [items]
  count.one = "{count} item"
  count.other = "{count} items"
  found = "{name} found {count} {item}!"
  ```

  Keys are dotted names (`menu.new_game`), unique per language. A key is identified at run time by its 32-bit **FNV-1a
  hash** of the full dotted name — the same hash spec 028's `UiText::name` uses — so `UiText::name("menu.new_game")` and
  `tr("menu.new_game")` meet without a generated header. The build refuses two different names with the same hash.
- **FR-17 Placeholders and plurals.** A string may contain named **placeholders** `{name}` (`{{` and `}}` for literal
  braces), in any order a translation needs. A key may have **plural forms** with the CLDR categories `zero`, `one`,
  `two`, `few`, `many`, `other`, selected by an integer argument named in the key's file entry (default: the first integer
  placeholder; the plan fixes the syntax). The engine ships the CLDR cardinal **integer** rules for every language it
  knows (at least the 12 languages a 3DS can be set to, plus Polish, Czech, Turkish, Ukrainian, Swedish, Danish, Norwegian,
  Finnish, Greek, Hungarian, Romanian, Catalan, Vietnamese, Indonesian, Thai… — the plan fixes the list and how a game
  supplies a rule for a language not in it). `other` is always required; the build requires every category the
  language's rule can produce.
- **FR-18 Language metadata.** A language file may give: its display **name**, its **font roles** (a map of role names
  the game chooses — `body`, `title` — to font assets; a language without a role uses the base language's), the number
  **grouping separator** and minimum digits for grouping (`1,000` / `1.000` / `1 000`, or none), and its **fallback
  language** (e.g. `pt-PT` → `pt-BR` → base). Plural rule and code come from the file name / declaration.

### Localization: what the build checks

- **FR-19 Checked by the build.** `nnh-assetc` checks every string file and stops the build with `file:line: reason`
  (spec 014's style, with "did you mean" for near-miss keys) on: invalid UTF-8 or TOML; a duplicate key; a key in a
  translation that the base language doesn't have; a placeholder in a translation that the base string doesn't have (or
  the plan's rule for plural-only placeholders); a missing `other` or a plural category the language needs; a malformed
  placeholder; a font role naming a font that doesn't exist; and **glyph coverage** — a character used by a string that
  isn't in the language's font for that role (or its fallbacks), naming the code point and the font. **Warnings** (the
  build goes on; an option or a Release preset may turn them into errors — Open questions): a key the base has but a
  translation lacks (the fallback is used; the warning lists the count and the first keys), combining marks or
  zero-width characters (FR-12), a string longer than a game-declared limit for its key prefix (optional). The checker's
  parser is bounded and fuzzed (constitution X).
- **FR-20 Converted tables.** Each language is converted into one compact binary table (keys sorted by hash, offsets,
  UTF-8 bytes, parsed placeholders and plural forms) read by a bounded validator at run time, the same bytes on PC and
  3DS. Key **names** are not in Release tables or Release binaries (only hashes; `scripts/check_release_strings.sh` stays
  clean); Debug/Development tables keep names for diagnostics. Editing one string file reconverts only that language
  (and the coverage of the fonts that depend on it).

### Localization: at run time

- **FR-21 The current language.** The localization state is module-level (like the audio mixer and the data index), with
  nothing loaded until the game asks: `loc::set_language(code)` loads that language's table (and its fallback chain's)
  into one tagged block (`strings` tag) sized from the largest language at build time, so a switch never grows memory.
  Queries: the current language's code, display name and index; the list of shipped languages (code, display name) for a
  language menu; whether a language is shipped. Errors are returned (`NotFound` for a language the game doesn't ship).
- **FR-22 Starting language.** `loc::start()` (or the first `tr` before any `set_language`, as the plan decides) picks,
  in order: a language the game passes (its saved setting, FR-26), else the **system language** (FR-27) matched against
  the shipped languages — exact code, then same language with another region (`pt` ↔ `pt-BR`), then the language's
  fallback chain — else the base language. The choice and why is logged once in Debug/Development.
- **FR-23 Text by key.** `tr(key)` (a dotted name, hashed at compile time when it is a literal, or a precomputed hash)
  returns the current language's UTF-8 text, stable until the next language switch takes effect (FR-25). A key missing in
  the current language uses its fallback chain, then the base language. A key no table has (a typo in code) returns a
  visible marker — the key's name in Debug/Development (logged once with the calling key), and a fixed short marker in
  Release (Open questions) — never a null pointer or a crash. Lookup allocates nothing and costs O(log n).
- **FR-24 Formatting.** `loc::format(out, capacity, key, args...)` writes the key's text with its placeholders replaced
  and its plural form chosen into the game's buffer, returning the length (truncated at a UTF-8 boundary with a
  Debug/Development warning when the buffer is too small). Arguments are named values: integers (formatted with the
  language's grouping, FR-18; no `printf`), UTF-8 strings (a player's name, an item's name — itself possibly `tr`'d), and
  another key's text. An argument the string doesn't use is ignored; a placeholder with no argument draws `{name}` in
  Debug/Development (logged) and nothing in Release. No allocation, no `printf`, the same bytes on every system. A
  format helper that fills a spec 028 borrowed buffer is a documented pattern.
- **FR-25 Switching language at run time.** `loc::set_language(code)` takes effect **at the start of the next frame**
  (before event dispatch, like scene changes), so pointers `tr` returned this frame stay valid until then; it then loads
  the new table into the block, publishes a **`LanguageChanged`** event (previous and new language index, ≤ 16 bytes) and
  bumps a **language serial** a game can poll. Loading the table is a load-time operation (a menu action), not a
  per-frame cost; its time is budgeted in AC-6.
- **FR-26 The player's choice.** The engine doesn't save the language itself: a game stores the code (or index) in its
  own settings (spec 027's settings type, as `examples/saves` does) and passes it at start. A "use the system language"
  choice is the game's (storing nothing). Documented pattern with a code example.
- **FR-27 The system language.** `nnh::system::language()` returns the system's language as a BCP 47-style code:
  - **3DS**: the console's language setting (the 12 values a 3DS can be set to: `ja`, `en`, `fr`, `de`, `it`, `es`,
    `zh-Hans`, `ko`, `nl`, `pt`, `ru`, `zh-Hant`), with a region added when the console's region decides it (Open
    questions: e.g. `pt-BR` on an American console);
  - **PC simulator**: a simulator setting remembered per game (spec 006's settings), overridable by an environment
    variable (`NNH_SIM_LANGUAGE=ja`), `en` by default so screenshots don't depend on the host machine (Open questions);
  - **a test script** (`nnh run --script`, Debug/Development): the script's `language <code>` command, which also
    switches the game's current language at the next frame if the game opted to follow the system language (FR-28) —
    the same on the console and with `--pc`, like the 3D slider's `slider` command.
  This is a small platform contract addition (one query per platform) and a new remote command (spec 013, D28).
- **FR-28 Following the system language.** A game may ask localization to follow `system::language()` (used by scripts
  and the simulator's setting): when it changes, the matched language is switched as by FR-25. Off by default (a game's
  saved choice wins).
- **FR-29 Font roles.** `loc::font(role)` returns the current language's font for a role (FR-18) as a borrowed font the
  localization state keeps loaded (fonts of the current language are loaded with the table and released when no longer
  used; the plan decides whether fonts shared by languages stay loaded), so `draw_text(…, {.font = loc::font("body")})`
  draws Japanese in a Japanese font and English in a Latin one with no per-language code.
- **FR-30 Widgets follow the language.** A documented two-line hook connects spec 028: `ui.set_resolver(loc::resolver())`
  (the resolver maps `UiText::name`/`UiText::key` hashes to `tr` text — the UI header doesn't include localization's)
  and, on `LanguageChanged`, rebinding the UI's font slots from `loc::font(role)` and calling `ui.text_changed()`. A
  helper may bundle it (Open questions). Choice options, list rows and the prompt helper's buttons take keys like every
  other text.
- **FR-31 Pseudo-localization.** In Debug/Development the build can generate a **pseudo-language** (`qps`, Open
  questions) from the base language: Latin letters replaced by accented look-alikes within Latin-1 (so the built-in font
  still draws them), each string padded by about 30 % and wrapped in brackets, placeholders kept — so untranslated
  (hard-coded) text, truncation and overflow are visible on screen. Selectable like any language; never in Release.

### Determinism, debugging, cost, safety, opt-in

- **FR-32 Same results everywhere.** Lookup, fallback, formatting, plural choice, measuring, wrapping and drawing depend
  only on the assets, the language and the arguments, in integer arithmetic; the fake platform, every PC host compiler
  and the 3DS give the same strings, the same draw items and the same pixels (FR-13).
- **FR-33 Debugging.** Debug/Development: a one-call dump of the current language (code, keys count, bytes, fallbacks,
  missing keys used so far) and of a font (glyphs, pages, bytes, metrics) to the log; a debug view that draws a font's
  pages; `NNH_LOC_SHOW_KEYS=1` (or a simulator toggle) makes `tr` return key names, to find which key a text came from.
  Compiled out of Release.
- **FR-34 No allocation while playing, bounded cost.** Fonts and tables allocate only when loaded or switched. `tr`,
  `format`, measuring, wrapping and drawing allocate nothing; a glyph lookup is O(log glyphs) or better; drawing costs
  one sprite per visible glyph as spec 005 (glyphs, shadows and outlines count against `max_draws`).
- **FR-35 Misuse is loud in Debug, harmless in Release.** A font used after it was destroyed, a scale outside 1–8, a
  `tr` before any string table exists, a format buffer of capacity 0, an unknown role, `set_language` for a language not
  shipped: assert or log in Debug/Development as the plan decides per case, and do the documented harmless thing in
  Release (built-in font, empty string, no change). Never undefined behaviour.
- **FR-36 Optional and light.** Fonts and localization are optional (XI) and separate: game fonts work without
  localization, localization works with the built-in font only (Latin-1 languages). Each has one light public header (no
  standard containers, no `<string>`); gfx's public header gains only the font option. Nothing in gfx, scenes or the UI
  includes the localization header. A game that uses neither links no new code: `examples/hello` keeps its size on PC
  and 3DS; a game using only the built-in font draws exactly as today.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform and on the host converter: UTF-8 decoding of every
  length, invalid, overlong and surrogate sequences; glyph lookup; metrics, advances and kerning; draw items of a
  string (pen positions, kerning, offsets, alignment, scale, zoom, shadow, outline, fallback fonts, replacement, hollow
  box, missing code points logged once); measure = drawn box (no pixel outside); wrapping (spaces, hyphens, `\n`, long
  words, CJK breaks, the line-breaking table, max lines and both ellipses) shared with spec 028's labels; spec 028's
  label tests with a proportional font; string lookup, fallback chains, the Release marker; formatting (named
  placeholders in any order, `{{`, integers with each grouping style, `INT_MIN`/`INT_MAX`, strings, nested keys,
  truncation at a UTF-8 boundary); plural rules for every shipped language against CLDR's sample integers; language
  matching for system codes; switch timing (pointers valid until the next frame, `LanguageChanged` once, serial); misuse
  cases. Converter tests: BMFont files (text form, pages, kerning, refusals), TTF/OTF descriptions (sizes, character
  sets, strings-driven sets, anti-aliasing on/off, outline, page limits, refusals), string files (every FR-19 error and
  warning with its exact `file:line: reason`), coverage errors. Fuzz targets for the BMFont reader, the string-file
  reader and the converted font and table validators. The Linux build VM runs them with GCC 13/14 and clang 18 and with
  ASan/UBSan, as previous specs did.
- **AC-2 Golden session.** A test renders a fixed set of texts (every test font, several languages including a CJK one,
  wrapped and unwrapped, with shadow and outline, formatted strings with plurals) into draw items and compares a
  checksum with a golden value; it passes on every host compiler and in the console self-check (AC-4). Converted font
  and table files of the fixtures have golden checksums too (the converter is deterministic on every host).
- **AC-3 Example `examples/localization`** (Open questions: name), modes chosen by `LOC_DEMO` (`showcase` (default) |
  `check` | `bench`): a title and menu (spec 028 widgets) in English, Português (Brasil), Deutsch, Русский and 日本語,
  switched by a language choice and by SELECT, each language using its fonts (a pixel BMFont for the HUD, a TTF body font,
  a Japanese font subset from the strings), a dialog box with wrapped text and typewriter reveal in every language, a
  shop line with plurals (`0/1/2/5/21 items` in each language), the pseudo-language in Debug, and a font gallery (each
  font's sample text, metrics and pages). It starts in the system language. The log prints one line per switch
  (`loc: language ja (system)`, `loc: language pt-BR (player)`). PC screenshots of each language and the gallery are
  checked by reading the PNGs.
- **AC-4 Console script.** The `check` mode runs AC-2's session and logs `loc: PASS <checksum>` or `loc: FAIL ...`
  against golden values recorded from the simulator. `examples/localization/tests/check.nnhs` starts it, expects the
  verdict, then in the showcase uses `language` commands and button presses to visit every language, expecting the
  logged lines and taking screenshots. On an Old 3DS the maintainer only opens the Homebrew Launcher and presses Y, and
  `nnh run localization --script examples/localization/tests/check.nnhs` passes; the same script passes with `--pc`.
  Started without a script, the console's own language setting picks the starting language (checked once by the
  maintainer with the console set to another language: yes/no).
- **AC-5 Pixels.** Simulator and console screenshots of the same states show text at the same pixel positions with the
  same glyph pixels (alpha pages blended per spec 004's rules; the plan records the comparison as spec 028 AC-5 did).
- **AC-6 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `loc bench: PASS <numbers>` (or FAIL). **Targets** (confirmed by the plan before code; then the maintainer's bench
  policy: optimise first, else set the budget from the first console measurement with margin):
  - drawing **400 glyphs** of a proportional game font with kerning: CPU cost per glyph **at most 1.15×** the built-in
    font's for the same count in the same run, and 400 glyphs + 300 game sprites at 60 Hz with no missed frame of 600;
  - `measure_text` + wrapping of a 400-character paragraph: **under 0.1 ms** (Latin) and **under 0.15 ms** (Japanese);
  - `tr` of 1000 keys: **under 0.1 ms**; `format` of 100 strings with two placeholders and a plural: **under 0.1 ms**;
  - switching language (a 64 KiB table and one 1024×1024 alpha page from romfs, then spec 028's `text_changed()` on the
    example's menu): **under 100 ms**, documented as a menu-time operation;
  - memory: the plan states bytes per glyph, per key, per font and the example's totals; a **Japanese body font** of the
    example's strings plus the 1006 kyōiku kanji and kana at 12–13 px fits in **one 1024×1024 page** (≤ 1 MiB A8), and the
    whole example (fonts + tables) stays **under 2 MiB** of linear memory.
  Spec 012's tile gate, the sprites bench, spec 028's UI bench and the platformer's 60 Hz replay still pass.
- **AC-7 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS; `examples/text`,
  `examples/sprites`, `examples/ui` and `examples/platformer` keep their screenshots pixel-identical and their scripts
  and goldens passing (spec 028's `30400DA6` included); every earlier spec's tests pass unchanged. A game that loads a
  font but has no string files links no localization code, and vice versa (checked on the 3DS map file or symbol list).
  Building a game `.cpp` that includes the new headers stays within constitution III's budgets; editing one string file
  reconverts only it and its dependent fonts, in under 2 s on the maintainer's PC for the example (measured before and
  after); a clean configure + build of the engine stays under 30 s.
- **AC-8 Release hygiene.** `scripts/check_release_strings.sh` passes on the example's Release `.elf` (no key names, no
  debug texts); the pseudo-language and key-name tables are absent from Release packages.
- **AC-9 Docs.** New `docs/api/fonts.md` (font assets, descriptions, character sets, metrics, drawing, outline, fallback,
  costs) and `docs/api/localization.md` (languages, string files, placeholders and plurals, build checks, `tr`/`format`,
  switching, the system language, font roles, widgets, pseudo-localization, saving the choice); a guide
  `docs/guides/localizing-a-game.md` (from one language to three, handing files to translators, adding a CJK language
  within Old 3DS memory, font licences); `docs/api/text.md`, `gfx.md`, `ui.md`, `assets.md`, `system.md`, `simulator.md`,
  `saves.md` and `docs/guides/testing-on-the-console.md` (`language`) updated; decision rows in
  `docs/architecture/decisions.md` (D49 fonts, D50 localization, numbers fixed by the plan); `docs/REFERENCES.md` citing
  the prior art below; `docs/tech/` pages for the vendored rasteriser; CLAUDE.md layout and ROADMAP updated.

## Non-goals

- **A run-time rasteriser** (TTF on the console) and **signed distance field fonts**: every glyph is baked by the build at
  the sizes a game asks for; the PICA200 has no fragment shaders for SDF edges and baked bitmaps keep pixels identical.
- **Text shaping**: right-to-left scripts (Arabic, Hebrew), Indic scripts, Thai line breaking by dictionary, ligatures,
  combining-mark positioning, vertical CJK text, colour emoji. (HarfBuzz-class shaping is far beyond an Old 3DS budget.)
- **The 3DS system font** in this spec (Open questions): it can't exist in the simulator (constitution IX), so it would
  break FR-13; a `<nnh/3ds/...>` extension is a possible follow-up.
- **Rich text and markup** (colour spans, inline icons, `[b]`, ruby/furigana) and per-character effects (wave, shake):
  a follow-up; the per-character measure (FR-8) lets a game draw its own.
- **Text input** (keyboards, `swkbd`), as spec 028.
- **Localized assets other than text** (images with words, voice lines): a documented pattern (paths built from
  `loc::code()`), not an engine remap table (Open questions).
- **Gender/select forms, ordinal plurals, dates, times, currencies and decimal numbers**: integers with grouping only;
  a game formats the rest itself (the formatter accepts its strings).
- **Locale-aware sorting and case mapping** (collation, `toupper` for every script).
- **Localizing the engine's own texts** (debug overlay, NNH Link, `nnh` tool messages): they stay English and the
  built-in font.
- **An editor, a translation management tool or import from spreadsheets / gettext PO / XLIFF** in this spec (Open
  questions: CSV import as a converter option).
- **Changing the built-in font or debug text**: spec 005's font, its 8×8 cells and its Latin-1 coverage stay as they are.

## Constraints

- **I** — portable core code; the only platform parts are the system-language query and, if chosen, an alpha texture
  format, added to the contract for every platform (pc, 3ds, fake). Games use no `#ifdef`; the same fonts, tables, draw
  items and pixels on PC and 3DS (FR-13, FR-32); the example and scripts run in the simulator and on the console.
- **II** — glyph pages are power-of-two textures ≤ 1024 with a page limit per font; alpha pages to cut memory 4×; one
  string table per language loaded at a time in a block sized at build time; CJK coverage subset from the game's strings
  so a Japanese font fits the Old 3DS (AC-6). Text is the expensive part of a frame on the 3DS (one sprite per glyph,
  about 12 µs each on an Old 3DS per the sprites bench), so budgets are stated with glyph counts.
- **III** — public headers stay light; fonts and tables are converted by `nnh-assetc` incrementally (depfiles), outside
  the "edit one game `.cpp`" path; the rasteriser is host-only (never in a game build); build times measured (AC-7).
- **IV** — state is queried (current language, `tr`, metrics); the language change is a fact published once
  (`LanguageChanged`) and a serial; the UI talks to localization only through the resolver function and the game's hook.
- **V** — no exceptions (`Result` from loading), no RTTI, no `virtual`, no allocation in `tr`, `format`, measuring,
  wrapping or drawing; no `printf` in those paths; switches allocate nothing (block sized at build time).
- **VI** — decoding, metrics, layout of runs, wrapping, lookup, fallback, formatting, plural rules and the converters
  unit-tested first; a golden session (AC-2); visible behaviour by screenshots and on the console (AC-3 to AC-5).
- **VII** — font loading, glyph lookup, run layout, wrapping, drawing, the string table, formatting, plural rules,
  language matching and each converter in small separate files; one public header per module (fonts, localization).
- **VIII** — header comments, two `docs/api/` pages, a guide, the linked pages and the example (AC-9).
- **IX** — no copyrighted fonts in the repository: the engine ships none besides spec 005's; test and example fonts are
  self-made (generated by a script in the repository, as `make_font.py` does) or permissively licensed (OFL 1.1 or
  similar) with the licence file next to them and named in `docs/REFERENCES.md`; the maintainer approves any third-party
  font file. The rasteriser is vendored, unmodified and pinned in `third_party/` with a compatible licence (public domain /
  MIT, as stb_vorbis), host-only. The 3DS system font is not used (it is Nintendo's). Formats (BMFont, CLDR rules) are
  implemented from their published documentation; no code copied.
- **X** — fonts and string files are external input: the BMFont reader, the description and string-file readers, the
  rasteriser's input and the run-time validators of converted fonts and tables are bounded and fuzzed; a malformed font
  file never crashes the build tool; run-time validators reject damaged assets with a `Result`.
- **XI, D25** — optional and separate (FR-36): fonts without localization, localization without game fonts, widgets
  without either; plain `draw_text` with the built-in font stays supported.
- **Under pressure:**
  - **Spec 028's text seam** assumes monospace cells (`measure → {cells, px}`, `cell_width`); FR-14 changes it to
    per-character advances with kerning. Spec 028 committed its seam to this; the plan amends the seam's internals, keeps
    every widget call and spec 028's golden.
  - **Same pixels (I, FR-13)** is why the 3DS system font and run-time rasterising are out; it also forces integer scales
    and build-time rounding of advances (no sub-pixel positioning).
  - **The platform contract** grows: a system-language query and possibly an alpha texture format (spec 005 FR-7's
    "texture from pixels" gains a format). Both are small but touch every platform library.
  - **Spec 013's remote protocol** gains a `language` command (D28), like the `slider` command of spec 026b.
  - **`max_draws` and glyph cost**: outlines and shadows double or triple the draws per glyph; a text-heavy CJK dialogue
    screen is the stress case. If AC-6's draw target is missed, the fallback is spec 028's T10.4 path (a glyph batch
    through D39's batch mechanism), not a contract change.
  - **Module-level state** for localization (a current language) is engine state like the audio mixer's, not a hidden
    global in game code; the game still owns its fonts and decides when to switch.
  - **Repository size and IX**: a CJK test font is megabytes before subsetting; the repository should hold only a small
    subset or a generated font (Open questions).
  - **Build tool dependencies (III, X)**: a vendored rasteriser is a new third-party parser of untrusted files, run on the
    developer's machine; it is fuzzed through the converter.

## Prior art

- **AngelCode BMFont** — the de-facto bitmap font interchange format (text, XML and binary forms; chars, pages,
  kerning pairs, line height, base) — [BMFont](https://www.angelcode.com/products/bmfont/),
  [file format](https://www.angelcode.com/products/bmfont/doc/file_format.html). FR-1's bitmap source.
- **stb_truetype** (Sean Barrett, public domain / MIT) — a single-file TrueType rasteriser with kerning, used by many
  engines' font bakers — [github.com/nothings/stb](https://github.com/nothings/stb). Candidate for FR-1's build-time
  rasteriser (Open questions); FreeType ([freetype.org](https://freetype.org), FTL or GPLv2) is the higher-quality
  alternative.
- **Godot** — font resources, `tr()`/`tr_n()`, translation CSVs with plural columns (4.6+) or gettext PO, the
  `TranslationServer` with locale matching and a fallback locale, pseudolocalization in project settings —
  [Internationalizing games](https://docs.godotengine.org/en/4.4/tutorials/i18n/internationalizing_games.html),
  [Localization using spreadsheets](https://docs.godotengine.org/en/latest/tutorials/i18n/localization_using_spreadsheets.html),
  [TranslationServer](https://docs.godotengine.org/en/stable/classes/class_translationserver.html). FR-17's plurals,
  FR-22's matching and FR-31 follow it.
- **Unity** — the Localization package (string tables per locale, smart strings with plural formatters, locale
  selectors: system language, command line, player preference) and TextMeshPro font assets (atlas baking with character
  sets from a text file). FR-3's strings-driven character sets and FR-22's selector order follow it.
- **Unicode CLDR** plural rules (cardinal categories zero/one/two/few/many/other) and **UAX #14** / **JIS X 4051** line
  breaking (kinsoku) — FR-17, FR-10.
- **libctru** — `CFGU_GetSystemLanguage` (the 3DS language setting, `CFG_Language`: JP, EN, FR, DE, IT, ES, ZH, KO, NL,
  PT, RU, TW) and the shared system font (`fontGetSystemFont`, a BCFNT per region: JPN/EUR/USA, CHN, KOR, TWN) —
  [cfgu.h](https://libctru.devkitpro.org/cfgu_8h.html), [font.h](https://libctru.devkitpro.org/font_8h.html),
  [3dbrew: System Font](https://www.3dbrew.org/wiki/System_Font). FR-27's 3DS source; the system font is discussed and
  left out (Open questions).

## Open questions

Answered 2026-11-25: the maintainer approved the spec with every question as recommended (the bold option in each; the example's Japanese font is the approved OFL subset with its licence). **Amended 2026-11-26 by the plan's question 1 (approved): glyph pages store 4-bit coverage (LA4 on the 3DS, same memory as A8) instead of A8; wherever this spec says A8, read 4-bit coverage. A8 may be added later on the 3DS side only.**

- [x] **One spec or two?** (a) **fonts and localization in one spec (029) with two optional modules**; (b) two specs
      (029 fonts, 030 localization). — **Recommended: (a)**. They meet in glyph coverage (FR-19), per-language fonts
      (FR-29) and the UI hook (FR-30); one example shows both; the ROADMAP's 1.0 list treats them as one item. Two
      headers keep them separately usable (FR-36).
- [x] **Font sources.** (a) **BMFont `.fnt` (text form; XML/binary if cheap) + TTF/OTF baked at build time from a
      description file**; (b) BMFont only (the game bakes TTFs with an external tool); (c) also image-grid fonts (a PNG
      with fixed cells, like the built-in). — **Recommended: (a)**. Baking TTFs in the build is what makes
      strings-driven CJK subsets (FR-3) and the coverage check (FR-19) practical; BMFont covers every pixel font tool.
      Grid fonts are a follow-up (a BMFont exporter does them already).
- [x] **Rasteriser.** (a) **stb_truetype vendored in `third_party/`, host-only**; (b) FreeType (better hinting, bigger,
      FTL licence needs an attribution line, CMake dependency); (c) an external tool the developer installs. —
      **Recommended: (a)**: one file, public domain/MIT like the vendored stb_vorbis, no install step, deterministic
      output pinned by golden checksums. Quality at small sizes is fine for unhinted anti-aliased text; pixel fonts use
      anti-aliasing off at their design size.
- [x] **Alpha pages (platform contract).** (a) **an alpha texture format (A8; A4 if the plan shows it blends identically)
      added to "texture from pixels" on every platform**; (b) RGBA only (4× memory). — **Recommended: (a) with A8**: a
      1024×1024 CJK page drops from 4 MiB to 1 MiB, which decides whether CJK fits an Old 3DS; the simulator blends A8
      the same way. A4 (another 2×) only if pixel equality is proven.
- [x] **Baked outlines.** (a) **optional outline glyphs baked by the TTF description, drawn first in an outline colour
      (one more draw per glyph)**; (b) not in this spec. — **Recommended: (a)**: outlined text over busy backgrounds is
      the most common game-font need after colour; the shadow alone isn't enough. 1–3 px.
- [x] **Fallback fonts.** — **Recommended: up to 3 fallback fonts per font, then the font's replacement glyph, then the
      hollow box** (FR-11). Lets a Latin font borrow a CJK font's glyphs for names without per-language code.
- [x] **Line breaking for CJK.** (a) **break between CJK characters plus a small fixed kinsoku table**; (b) spaces only (as
      spec 028 today). — **Recommended: (a)** (FR-10): Japanese and Chinese dialogue is unreadable otherwise; the table is
      tens of code points and the same on every system.
- [x] **String file format.** (a) **one spec 014 TOML-subset file per language** (`strings/en.strings`, keys as dotted
      names, plural forms as `key.one`/`key.other`); (b) one CSV with a column per language (Godot); (c) gettext PO. —
      **Recommended: (a)**, with a CSV → per-language import as a possible converter option later. Reuses spec 014's
      reader, errors and fuzzing; per-language files are what translators get and what version control diffs well;
      hand-written, as constitution XI asks.
- [x] **Keys: hashed names or a generated header?** (a) **dotted names hashed with FNV-1a at compile time (the same
      hash as `UiText::name`), checked by the build for collisions; a code typo shows a marker at run time (FR-23)**; (b) a
      build-generated header of key constants (compile errors for typos, but generated code the game compiles against and
      a rebuild of game code when strings change). — **Recommended: (a)**: no generated code (spec 014's rule), editing
      strings never recompiles the game (III), and it meets spec 028's existing keys. Debug logs unknown keys once.
- [x] **Unknown-key marker in Release.** (a) **a fixed short marker (`??`)**; (b) an empty string; (c) the base language's
      text if any. — **Recommended: (a)**: visible to a tester, harmless to the player; Debug shows the key's name.
- [x] **Missing translations: warning or error?** — **Recommended: a warning by default (the fallback is used, the count
      and first keys listed), an error when the game sets a strict option** (e.g. for its release builds). Partial
      translations are normal during development.
- [x] **Plural rules shipped.** — **Recommended: CLDR integer cardinal rules for the 12 3DS languages plus about 20 common
      ones, in a small table; a game may supply a rule for another language in its language file (a tiny integer
      expression, checked by the build)**; the plan fixes the syntax.
- [x] **Where localization state lives.** (a) **module-level state (current language, one block), like audio and the data
      index; fonts stay game-owned**; (b) a game-owned `Localization` object passed around. — **Recommended: (a)**: `tr`
      is called from everywhere (scenes, UI resolver, data-driven text), a single current language is what games have,
      and the block is sized at build time; a game-owned object adds plumbing for no gain.
- [x] **When a switch takes effect.** — **Recommended: at the start of the next frame, with a `LanguageChanged` event and a
      serial** (FR-25), so `tr` pointers are valid for the whole frame they were taken in, like scene changes.
- [x] **Saving the player's language.** — **Recommended: the game's job, in its spec 027 settings (documented pattern)**
      (FR-26); the engine saves nothing on its own (constitution X: the game owns its files).
- [x] **3DS system language and region.** (a) **report the language setting's code, adding a region only where the
      console's region decides it (e.g. `pt` on an American-region console → `pt-BR`, `es` → `es-419`, `fr` → `fr-CA`;
      `en` → `en-US`/`en-GB`), matching falls back to the language alone**; (b) language only. — **Recommended: (a)**, with
      the exact region mapping verified on hardware by the plan (the maintainer's console is one region; the table is
      from libctru/3dbrew documentation).
- [x] **Simulator default language.** (a) **`en`, changed by the simulator's remembered setting or `NNH_SIM_LANGUAGE`**;
      (b) the host operating system's locale. — **Recommended: (a)**: screenshots and goldens must not depend on the
      developer's machine; an opt-in "use the host locale" setting is cheap if wanted.
- [x] **The 3DS system font.** (a) **not in this spec**; (b) a `<nnh/3ds/...>` extension returning a font from the shared
      system font (no download, CJK for free; but the simulator can't have it, pixels and metrics differ, and its glyph
      sheets are A4 BCFNT); (c) a portable option that falls back to a game font on PC. — **Recommended: (a)**, follow-up
      (b) if a game asks: it breaks FR-13 and can't be tested in the simulator; the font is Nintendo's (used from the
      console, never shipped, so IX holds for (b), but it can't be a test fixture).
- [x] **UI hook.** (a) **a documented two-line pattern plus an optional small helper (`loc::bind(ui, roles…)`) linked only
      when called**; (b) the UI subscribes to `LanguageChanged` itself. — **Recommended: (a)**: the UI must not depend on
      localization (XI, spec 028 FR-33).
- [x] **Pseudo-localization.** — **Recommended: yes, generated by the build from the base language in Debug/Development
      only, code `qps` (Microsoft's convention; Godot's toggle), Latin-1 look-alikes so the built-in font draws it**
      (FR-31). Cheap and the fastest way to find hard-coded strings.
- [x] **Localized assets (images, sounds).** — **Recommended: a documented pattern (`"art/title_" + loc::code()` with a
      base fallback) and no engine remap table in this spec**; Godot's remaps are a follow-up if games repeat the code.
- [x] **Font licences in the package.** — **Recommended: the description's `licence` file is copied into the PC folder
      and the romfs (`licences/<font>.txt`), and listed by the build**; the plan decides the path. OFL requires the
      licence to travel with the font.
- [x] **Test and example fonts.** (a) **test fixtures self-made: BMFont files and pages generated by a script in the
      repository, and a tiny TTF generated by a script (or a few-glyph subset of an OFL font with its licence); the
      example's Japanese font a small OFL subset (e.g. M PLUS or Noto Sans JP, subset to the example's strings + kyōiku
      kanji, ≤ 300 KB) committed with its OFL text, approved by the maintainer**; (b) no CJK in the repository (CJK checked
      only by unit tests with a synthetic font). — **Recommended: (a)**: AC-6's CJK memory claim needs a real font on the
      console; a subset keeps the repository small. The maintainer approves the exact font and licence (IX).
- [x] **Example name and the existing `examples/ui`.** — **Recommended: a new `examples/localization` (fonts + languages);
      `examples/ui` keeps its hand-written English/Português resolver and its golden** (adopting `loc` there would change
      its golden for no new coverage); `examples/text` gains nothing.
- [x] **Old 3DS cost targets** — **Recommended: AC-6's targets, confirmed by the plan, then the maintainer's bench
      policy** (optimise, else set from the first console measurement with margin).
- [x] **Decision rows** — **Recommended: D49 ("game fonts: baked at build time into alpha/colour pages ≤ 1024, drawn as one
      sprite per glyph through the existing text path, integer metrics") and D50 ("localization: per-language string files
      checked and converted by the build, FNV-1a keys, module-level current language switched at frame start, CLDR
      integer plurals")**, written by the plan.
