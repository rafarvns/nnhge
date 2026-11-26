# Plan 029: Game fonts and localization

Status: implemented 2026-11-26 (T9 and T10.2 pending; see "Implementation notes"); approved 2026-11-26 (the maintainer, the five questions at the end as recommended); drafted 2026-11-25 (the spec was approved 2026-11-25 with every open question as
recommended, and those recommendations are this plan's decisions; five plan-level questions at the end)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decisions D49, D50 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 005 (`gfx::draw_text`/`measure_text`, the built-in font, `detail::next_codepoint`, the hollow box, one sprite
per glyph), spec 004 and D33 (the draw list, layers, `ignore_camera`, zoom; texture slots and generations; D47's
deferred frees), spec 007 (assets by path, `asset_open`/`asset_read_at`, romfs/PC folder, per-file build steps), spec 014
and D27/D29 (`nnh-assetc`, its TOML-subset reader, `file:line: reason`, "did you mean", FNV-1a names in
`nnh/detail/name_hash.hpp`), spec 028 and D48 (the text seam `src/ui/ui_text_seam.*`, `ui_wrap.*`, `UiTextLook::font`,
`UiText::name`, `set_resolver`, `text_changed()`, `ui_draw_out`'s draw hash), spec 013 and D28 (the remote protocol and
`.nnhs` scripts; spec 026b's `Slider` message as the model), spec 017 and D32 (the after-input hook list), spec 002
(`Result`, tagged memory, logs, `nnh::system`), spec 001 (the bus). Built in one worktree (branch `spec-029`) from `main`
at `f3e16bb`; no other spec is built in parallel.

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure point: the two contract additions (a texture from coverage, the system language) touch every platform library. Answered: both are one function each, in their own source file per platform (pc, 3ds, fake), with conformance tests; everything else is portable core. Glyph bitmaps are baked by the build, so every system draws the same bytes; pen positions, kerning, wrapping, the ellipsis, lookup, plural rules and number formatting are integer code; the only floats are the existing float position the game passes and gfx's float placement of each sprite (unchanged). AC-2's golden runs on WinLibs GCC, GCC 13/14, clang 18, the 32-bit host build and the console; the converter's outputs have golden checksums on every host |
| II 3DS constraint | ⚠️→✅ | Pressure points: CJK memory, glyph cost, AC-6's targets set before any measurement. Answered: pages of 4-bit coverage (1 byte per pixel in GPU memory, 2 pixels per byte on disk, question 1), character sets computed from the game's strings, ≤ 8 power-of-two pages ≤ 1024 per font, 20 B per glyph; one string block per language chain sized by the build, so a switch never grows memory; a glyph is one sprite as today (the game font adds a lookup, ≈ 0.3–0.6 µs on ≈ 12 µs). Four AC-6 targets are tight or likely missed on the first run (estimates below, made with plan 028's lessons); the optimisation round and the bench policy cover them |
| III Build speed | ✅ | Two light public headers (`<nnh/font.hpp>`, `<nnh/localization.hpp>`: `<cstdint>`, `<nnh/result.hpp>`, `<nnh/gfx.hpp>`, `name_hash.hpp`; no standard containers, `<string>` or UI header); `<nnh/gfx.hpp>` gains ≈ 30 lines. Fonts and strings are converted by `nnh-assetc` per file with depfiles, outside the "edit one game `.cpp`" path; the rasteriser and PNG decoder are host-only (never in a game build). Budgets: each new header within +400 preprocessed lines and +0.05 s of T0.1's `gfx.hpp + result.hpp`; one string edit reconverts in < 2 s (T8.4) |
| IV Event-driven | ✅ | State is queried (current language, `tr`, metrics, serial); the switch is one fact, `LanguageChanged` (8 B), published once at the frame it takes effect; the UI talks to localization only through a resolver function pointer and the game's own two-line hook (or `loc::apply`); gfx and the UI never name the localization module |
| V No hidden costs | ✅ | No exceptions (`Result` from loading and starting), no RTTI, no `virtual`; no allocation in `tr`, `format`, measuring, wrapping or drawing; no `printf` in those paths (integers formatted by hand); loading and switching allocate only at load time, into blocks sized by the build |
| VI Spec + test first | ✅ | Every rule tested first on the fake platform and in the converter's tests: decoding, break classes, the font format and validator, lookups, runs, wrapping (spec 028's cases kept), drawing items, measuring, the table format, lookup chains, formatting, plural rules against CLDR's sample integers, matching, switch timing; AC-2's golden; fuzz targets for every parser; a planned hand-mutant pass |
| VII Small files | ✅ | Shared text code in `src/text/` (6 new files), module `font` (10 sources), module `loc` (13 sources), each one concept and ≤ 250 lines; `nnh-assetc` gains ≈ 26 sources; one public header per module |
| VIII Documented API | ✅ | `docs/api/fonts.md`, `docs/api/localization.md`, `docs/guides/localizing-a-game.md`; `text.md`, `gfx.md`, `ui.md`, `assets.md`, `system.md`, `simulator.md`, `saves.md`, `testing-on-the-console.md`, `docs/tech/` pages for the vendored libraries; `examples/localization`; D49, D50 |
| IX Clean and legal | ⚠️→✅ | Pressure point: third-party fonts and code. Answered: `stb_truetype.h` (and, question 2, `stb_image.h` for BMFont pages) vendored unmodified and pinned in `third_party/`, public domain/MIT, host-only; test fonts generated by `scripts/make_test_fonts.py` (original, zlib); the example's HUD pixel font generated by its own script; one OFL 1.1 font subset (≤ 300 KB, renamed, `OFL.txt` beside it, question 3) approved by the maintainer; CLDR plural rules (Unicode License v3, data facts) and JIS X 4051/UAX #14 classes implemented from their published documents; BMFont from AngelCode's format page; credited in `docs/REFERENCES.md` |
| X Safe by default | ⚠️→✅ | Pressure point: three new parsers of external files on the developer's machine (BMFont text/XML, TTF/OTF through stb_truetype, PNG) and two on the console (converted fonts and tables). Answered: every reader bounded; a pre-validator of every TrueType/CFF offset stb_truetype follows (its README says it does no range checking) plus a zero-padded copy; libFuzzer targets through the converter for BMFont, TTF/OTF, PNG pages, descriptions and string files; core validators of `.nnhfont`, `.nnhstr` and `.nnh_loc` shared with the converter, fuzzed, rejecting damaged assets with a `Result` |
| XI Opt-in, never in the way | ✅ | Fonts and localization are separate and optional: fonts without string files, string files with the built-in font, widgets with either or neither. `hello` (3ds-release and pc-release) stays **byte-identical** (no file it links changes, T0.1/T8.3); a game that never calls `load_font` links no font code, one that never calls `loc::` links no localization code (map files, T8.3) |

## Design

### Overview

```
<nnh/gfx.hpp>   TextOptions gains: FontRef font, wrap_width, max_lines, outline_color; measure_text(utf8, options),
                measure_chars, font_metrics(FontRef)        — the built-in path of draw_text is untouched
<nnh/font.hpp>  Font (RAII owner of a font table entry), load_font(path) → Result<Font>
<nnh/localization.hpp>  loc::start / set_language / tr / format / font(role) / apply(Ui&) ; LanguageChanged
<nnh/system.hpp>  system::language()

build (host)    nnh-assetc font     <x>.font (TOML: TTF/OTF + size + chars) | <x>.fnt (BMFont + <x>_N.png) → <x>.nnhfont
                nnh-assetc strings  strings/<code>.strings | strings/<code>/*.strings (+ base) → strings/<code>.nnhstr
                nnh-assetc strings-index  every table + every font → .nnh_loc (manifest, coverage, pseudo qps)
run time        text/  break classes, faces (built-in or font block), runs, the shared wrapper, measuring, drawing
                font/  format + validator (shared with nnh-assetc), page codec, table of 32 fonts, load, lookups
                loc/   table + manifest validators (shared), state, matching, tr, format, numbers, plural interpreter,
                       switch (a D32 after-input hook), roles → fonts, pseudo/debug
                ui/    the seam in pixels; font slots (ui_fonts.cpp, linked only when set_font is called)
contract        video.hpp: texture_create_alpha (own file per platform)   system.hpp: system_language() (own file)
remote (D28)    message type 8 `Language`; script command `language <code>|off`
```

### Decision D49: game fonts — baked by the build into 4-bit coverage (or colour) pages ≤ 1024, drawn as one sprite per glyph through the existing text path, integer metrics

**Assets.** A game font is one face at one pixel size, converted by `nnh-assetc font` into `<path>.nnhfont`, loaded by
`load_font("<path>")` from either source: a **font description** `<path>.font` (spec 014's TOML subset: an outline font
file, one size, a character set, anti-aliasing, padding, an optional baked outline, fallback fonts, a licence) whose
TrueType or CFF outlines are rasterised once on the host by the vendored, unmodified `stb_truetype`; or an AngelCode
**BMFont** `<path>.fnt` (text or XML form) with its pages `<path>_<n>.png` beside it, decoded on the host. One
description makes one asset (one size); a second size is a second small description (decision 4, question 4). Source
files (`.ttf`, `.otf`, `.fnt`'s pages, `.chars` lists) are pipeline-owned and never packed; the licence file named by a
description is an ordinary packed asset.

**Pages.** Glyphs (and their outline glyphs) are packed by a deterministic shelf packer into ≤ 8 pages per font, each a
power-of-two square or 2:1 rectangle from 8 to 1024, with ≥ 1 pixel of padding (set by the description, default 1).
Mono pages hold **coverage rounded to 16 levels** at build time (`a4 = (a × 15 + 127) / 255`; with anti-aliasing off,
`a ≥ threshold → 15`), stored two pixels per byte and run-length coded; they are created through a new contract function
`texture_create_alpha` (one byte of coverage per pixel, multiples of 17), which every platform draws as white texels of
that alpha tinted by the unchanged multiply path: **GPU_LA4 (luminance 15) on the 3DS, 1 byte per pixel; RGBA8 on the PC;
a copy on the fake**. A 1024×1024 page costs 1 MiB of linear memory, as A8 would, without a new 3DS draw path
(question 1). BMFont pages that carry colour (any texel whose colour isn't white or grey-as-alpha) stay RGBA8 through the
existing `texture_create_rgba`. Pages are gfx textures in the game's `max_textures` slots, accounted under the `font` tag.

**Glyphs and metrics.** Per glyph, 20 bytes: the code point (a sorted `u32` array) and a 16-byte record (page rectangle,
offset from the line's top-left pen, advance, page, line-break class bits, kerning run). Integer metrics, rounded once by
the converter: `px = round_half_away(units × size / units_per_em)` in 64-bit integers for advances, kerning, ascent,
descent and line gap (`size` is the em size in pixels; line height = ascent − descent + line gap); bitmap boxes from
stb_truetype's integer boxes; a BMFont's values are already integers. Kerning pairs (4 B: the right glyph's index and an
`int8`) are stored per left glyph in a run sorted by the right index. At load the font block derives a Latin-1 table
(256 × `u16` glyph indexes) and, for fonts with glyphs above U+00FF, a BMP index of 2048 `u16` buckets (`cp >> 5`), so a
lookup is a table read for Latin-1 and a bucket plus ≈ 1–3 compares for CJK.

**Drawing.** `gfx::draw_text` keeps spec 005's built-in path byte for byte; options with a font, a wrap width, a line
limit or an outline colour go to one shared run layout: each visible glyph is one sprite command of its page (pen +
offset, × scale, colour as tint, layer, `ignore_camera`, zoom per D33), the pen advancing by `advance + kerning(prev,
cur)`; a baked outline glyph is recorded first in `outline_color`, the shadow before both. Missing characters: up to 3
fallback fonts (metrics from the font that has the glyph, line height from the first), then the font's replacement
glyph (U+FFFD or `?`, chosen at build), then a hollow box at the font's size; Debug/Development logs each missing code
point once per font. **Fonts are reached through a table of 32 entries** (refcounted by path hash, so loading a path
twice shares one block); a `FontRef` (slot + generation, 0 = built-in) is what `TextOptions` and the UI hold, so a
destroyed font is detected (Debug assert, Release built-in font). gfx and the UI reach the table only through
`detail::g_font_resolve`, a function pointer set by the first `load_font` — no static initialiser, so a game that never
loads a font links no font code.

**Rejected:** a run-time rasteriser and SDF fonts (non-goals), 8-bit A8 pages (a new 3DS blend path, tint-mode switches
per glyph run and a few bytes more in `hello`'s `draw_2d`: question 1), RGBA pages for mono fonts (4× memory), page
images converted by the per-platform texture pipeline (two texture formats per font, platform-dependent assets), a
generated header of glyph indexes, sub-pixel pen positions, FreeType (licence line, size, a CMake dependency), one
description making several sizes (the asset index is built at configure time from file names: question 4), a hash
table for glyphs (the sorted array plus buckets is smaller and as fast for ≤ 65 535 glyphs), the 3DS system font
(non-goal).

### Decision D50: localization — per-language string files checked and converted by the build, FNV-1a keys, module-level current language switched at frame start, CLDR integer plurals

**Declaration and files.** A game lists its languages in `nnh_add_game(... LANGUAGES en pt-BR de ru ja [BASE en]
[STRICT_STRINGS])` (BASE defaults to the first). Each language's strings are `strings/<code>.strings` or every file of
`strings/<code>/` — spec 014's TOML subset with a **strings mode** that adds dotted keys, multi-line basic strings and a
1 MiB limit (still valid TOML); a reserved `[language]` table holds metadata (display name, `fallback`, `grouping`,
`grouping_min`, `[language.fonts]` roles, `[language.plural]` for a language the engine doesn't know, `[language.limits]`
in the base). Keys are dotted names; plural forms are `key.zero|one|two|few|many|other` (+ an optional `key.plural =
"<placeholder>"`); placeholders are `{name}`, `{{`/`}}` for braces.

**Build.** `nnh-assetc strings` checks one language against the base and writes `strings/<code>.nnhstr` (keys sorted
by FNV-1a 32 hash with a bucket index, string records, placeholder records, UTF-8 text; key names only in
Debug/Development); `nnh-assetc strings-index` checks what spans languages and fonts (hash collisions across every key,
font roles naming real fonts, **glyph coverage** of every string by its role's font and fallbacks, fallback chains),
writes the manifest `.nnh_loc` (languages, display names, chains, compiled plural rules, grouping, roles, the largest
chain's bytes) and, in Debug/Development, the pseudo-language `qps` generated from the base. Errors stop the build with
`file:line: reason`; missing translations and length limits are warnings unless `STRICT_STRINGS`. The CLDR 46 integer
cardinal rules of 36 languages live in the converter as CLDR rule text and are compiled into a small bytecode in the
manifest; a game's own rule uses the same syntax and compiler. Core has only the interpreter.

**Run time.** Localization is module-level engine state (like the mixer and the data index), empty until `loc::start()`,
which reads the manifest, allocates one block of the largest chain's bytes (tag `strings`), matches the starting language
(the game's choice, else the system language — exact, then the same language with another region, then base) and loads
the chain at once. `tr(key)` binary-searches nothing: bucket by the hash's top bits, then ≈ 1–2 compares per table of
the chain; a key no table has returns its name (Debug/Development, logged once) or `??` (Release). `format` writes into
the game's buffer: literal spans, named arguments (integers with the language's grouping, UTF-8 strings, another key's
text), the plural form chosen by the interpreter; truncated at a UTF-8 boundary. `set_language` validates at once and
takes effect **at the start of the next frame** through a D32 after-input hook: the block is overwritten with the new
chain, the role fonts are loaded (shared ones kept), `LanguageChanged` is published (delivered in that frame's dispatch)
and the serial bumped; every pointer `tr` returned stays valid until then. Following the system language
(`follow_system(true)`) uses the same hook. The block ends with a zero guard, so a stale `tr` pointer read after a switch
shows other text but never reads outside the block. Role fonts are loaded only when the game opted in (`use_fonts()`,
`font(role)` or `apply(ui, …)` — `loc_fonts.cpp`), so a localized game with the built-in font links no font code.

**Rejected:** a game-owned localization object (plumbing for one current language), generated key headers (game
recompiles when strings change), CSV/PO input (follow-up converter option), runtime TOML, `printf` formatting, per-language
plural code in core (36 functions linked into every localized game), a switch taking effect mid-frame (pointers die under
the game's feet), the UI subscribing to `LanguageChanged` itself (UI → localization dependency), engine-saved language
choice (the game's settings, spec 027), names in Release tables, double-buffered blocks (2× memory for a menu-time
operation).

### Public API (sketch)

```cpp
// <nnh/gfx.hpp> — additions only (the existing declarations are unchanged)
namespace nnh {
class Font;
/// A game font by handle (font table slot + generation); 0 = the built-in font. Cheap to copy; stale after the Font dies.
struct FontRef {
    std::uint32_t id = 0;
    constexpr FontRef() = default;
    FontRef(const Font& font);                 ///< NOLINT: implicit on purpose ({.font = body}); defined in the font module
    constexpr bool builtin() const { return id == 0; }
};
struct TextOptions {
    Color color = colors::white; int scale = 1; TextAlign align = TextAlign::Left; bool shadow = false;
    Color shadow_color = rgba(0, 0, 0, 160); int layer = 0; bool ignore_camera = false;      // unchanged (spec 005)
    FontRef font{};                    ///< D49: a game font (default: built-in)
    int wrap_width = 0;                ///< > 0: break lines to this width in pixels (FR-10)
    int max_lines = 0;                 ///< > 0: at most this many lines; the last ends with the ellipsis if cut
    Color outline_color{0, 0, 0, 0};   ///< alpha > 0: draw the font's baked outline glyphs first in this colour
};
/// Integer metrics at scale 1 (the built-in font: size 8, line 10, ascent 8, descent 0, no kerning).
struct FontMetrics { int size = 8, line_height = 10, ascent = 8, descent = 0, glyphs = 0, pages = 0;
                     std::size_t bytes = 0; bool outline = false, colour = false; };
/// Per code point of a measured text (FR-8): byte offset, pen position of its cell and advance, line.
struct TextCaret { std::int32_t byte = 0; std::int16_t x = 0, y = 0, advance = 0, line = 0; };
namespace gfx {
Vec2 measure_text(const char* utf8, const TextOptions& options);   ///< the box the drawn glyphs use (FR-8)
int measure_chars(const char* utf8, const TextOptions& options, TextCaret* out, int capacity); ///< count written
FontMetrics font_metrics(FontRef font = {});
} // namespace gfx
} // namespace nnh

// <nnh/font.hpp> — <cstdint>, <nnh/gfx.hpp>, <nnh/result.hpp>. Optional (XI).
namespace nnh {
struct MemTag;
inline constexpr int kMaxFonts = 32;          ///< loaded fonts at once (a path loaded twice counts once)
inline constexpr int kMaxFontPages = 8;
inline constexpr int kMaxFallbackFonts = 3;
class Font {
public:
    Font() = default; ~Font(); Font(Font&&) noexcept; Font& operator=(Font&&) noexcept;   // not copyable
    bool valid() const;
    FontRef ref() const;
    FontMetrics metrics() const;
    bool has(char32_t cp) const;                       ///< this font only
    bool covers(char32_t cp) const;                    ///< this font or one of its fallbacks
    int advance(char32_t cp) const;                    ///< scale 1, whole pixels; missing: the hollow box's advance
    int kerning(char32_t left, char32_t right) const;  ///< scale 1; 0 when the pair has none
    const char* path() const;                          ///< Debug/Development: the asset path; Release: ""
    void dump() const;                                 ///< Debug/Development: glyphs, pages, bytes, metrics
private:
    std::uint32_t id_ = 0;
};
/// Loads <path>.nnhfont (and its fallback fonts): one block (tag `font` by default) + its pages as textures. Load time.
/// Errors: NotFound, InvalidData (the validator's reason logged), OutOfMemory, Unsupported (32 fonts loaded).
Result<Font> load_font(const char* path, MemTag* tag = nullptr);
namespace font {
void debug_draw_pages(Screen screen, FontRef font, Vec2 at, int page = -1);  ///< Debug/Development; Release: nothing
} // namespace font
} // namespace nnh

// <nnh/localization.hpp> — <cstdint>, <nnh/gfx.hpp>, <nnh/result.hpp>, <nnh/detail/name_hash.hpp>. Optional (XI).
namespace nnh {
class Ui; struct MemTag;
/// Published when a switch took effect (FR-25), delivered in that frame's dispatch. previous = kNoLanguage at start.
struct LanguageChanged { std::uint8_t previous = 0xFF, current = 0xFF; std::uint16_t reserved = 0; std::uint32_t serial = 0; };
namespace loc {
inline constexpr std::uint8_t kNoLanguage = 0xFF;
inline constexpr int kMaxLanguages = 32;
inline constexpr int kMaxRoles = 8;
/// A key: the FNV-1a 32 hash of its dotted name, computed at compile time for a literal (the same as UiText::name).
struct Key {
    std::uint32_t hash = 0;
#if !defined(NNH_FLAVOR_RELEASE)
    const char* name = nullptr;        ///< for the Debug/Development marker and logs; absent in Release
#endif
    template <std::size_t N> consteval Key(const char (&name)[N]);
    constexpr explicit Key(std::uint32_t hash);
};
Key key(const char* name);             ///< a run-time name (data-driven text), hashed now
struct StartOptions { const char* language = nullptr; /* the player's saved choice, or null */
                      bool follow_system = false; MemTag* tag = nullptr; };
Result<void> start(const StartOptions& options = {});   ///< load time; NotFound: no .nnh_loc; InvalidData; OutOfMemory
void stop();                                            ///< frees everything (also at nnh::shutdown())
Result<void> set_language(const char* code);           ///< NotFound if not shipped; takes effect at the next frame
void follow_system(bool on);
const char* code();  const char* name();  int index();  std::uint32_t serial();
int language_count();  const char* language_code(int i);  const char* language_name(int i);  int find(const char* code);
const char* tr(Key key);                                ///< never null; valid until a switch takes effect
const char* resolve(std::uint32_t key);                 ///< tr by hash: ui.set_resolver(&loc::resolve)
struct Arg { std::uint32_t name = 0; std::uint8_t kind = 0; long long number = 0; const char* text = nullptr; };
template <std::integral T> constexpr Arg arg(Key name, T value);
Arg arg(Key name, const char* utf8);
Arg arg_key(Key name, Key key);                         ///< another key's text (unformatted)
int format(char* out, int capacity, Key key, const Arg* args, int count);   ///< bytes written (NUL added if room)
template <typename... A> int format(char* out, int capacity, Key key, const A&... args);
// Fonts (FR-29, FR-30): loc_fonts.cpp, linked only when one of these is called.
void use_fonts();                                       ///< load role fonts at start and every switch
FontRef font(Key role);                                 ///< the current language's font for a role (built-in if none)
struct UiFontRole { int slot = 0; Key role; };
void apply(Ui& ui, const UiFontRole* roles, int count); ///< resolver, font slots, text_changed() — loc_ui.cpp
template <std::size_t N> void apply(Ui& ui, const UiFontRole (&roles)[N]);
// Debug/Development (FR-33); Release: nothing.
void dump();  void set_show_keys(bool on);              ///< also NNH_LOC_SHOW_KEYS=1 (PC) at start
} // namespace loc
} // namespace nnh

// <nnh/system.hpp> — addition
namespace nnh::system {
/// The system's language as a BCP 47-style code ("ja", "pt-BR", "zh-Hant"), never null: a test script's `language`
/// while one is set, else the console's setting (3DS) or the simulator's (PC: NNH_SIM_LANGUAGE, language.ini, "en").
const char* language();
}

// <nnh/ui.hpp> / <nnh/detail/ui_style.hpp> — additions
enum class UiFont : std::uint8_t { Builtin, Slot0, Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7 };
inline constexpr int kMaxUiFonts = 8;
void Ui::set_font(int slot, FontRef font);   ///< ui_fonts.cpp; relayouts the roots; the game keeps the font alive
```

Exact names may still change while building; stage notes record any change.

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| `LanguageChanged` | localization, at the start of the frame a switch takes effect (and once after `start()`) | previous and current language index, serial (8 B) | scenes calling `loc::apply(ui_, roles)`; HUDs re-formatting cached strings; audio picking voice banks by `loc::code()` |

### Semantics fixed by the plan

**Font descriptions** (`tools/assetc/src/font_desc.*`; spec 014's reader, normal mode). Keys: `source` (required: a
`.ttf`/`.otf`/`.ttc` in the assets folder, relative to the description; `face = 0` for collections), `size` (required,
6..128: the em size in pixels), `chars` (array of named sets and `"U+XXXX"` / `"U+XXXX..U+YYYY"`; default `["ascii"]`),
`chars_from` (`.chars` files: UTF-8 text whose characters are added; never packed), `strings` (language codes whose
string files' characters are added — FR-3/FR-19), `antialias = true`, `threshold = 128` (with anti-aliasing off),
`padding = 1` (1..4), `outline = 0` (0..3 pixels), `fallback` (≤ 3 font asset paths), `replacement = "auto"` (`"auto"`:
U+FFFD if the source has it, else `?`; `"none"`), `licence` (required for outline fonts: a file in the assets folder, or
`"own"`), `page = 0` (0: the smallest that fits; else a forced side 64..1024). Named sets: `ascii`, `latin1`,
`latin-ext-a`, `latin-ext-b`, `latin-ext-additional`, `greek`, `cyrillic`, `armenian`, `georgian`, `general-punctuation`,
`currency`, `arrows`, `box-drawing`, `cjk-punctuation`, `hiragana`, `katakana`, `kana` (both + U+30FC), `fullwidth`,
`hangul` (11 172 syllables), `jis-level1` (2 965 kanji, from the JIS X 0208 row/cell table), `kyoiku` (the 1 006 kyōiku
kanji, pre-2020 list), `language-names` (every shipped language's display name). The converter rasterises only code
points the source maps (a set member the source lacks is skipped and counted: `fonts/body_16.font: 12 characters of
"latin-ext-a" are not in NotoSans.otf (first: U+0149)` as a note) — except characters needed by `strings`, which are
errors at the index step if no fallback covers them. The step prints `font fonts/body_ja_13: 1384 glyphs, 1 page
1024x512 (A4), 542 KB, licence licences/OFL-NNHExampleSansJP.txt`.

**BMFont** (`bmfont_text.*`, `bmfont_convert.*`): text and XML forms through one attribute tokenizer (`key=value`,
quoted strings, bounded lines); the binary form is refused (`fonts/hud.fnt: binary BMFont files aren't read: export the
text or XML form`). Read: `info` (size, padding, outline), `common` (lineHeight, base, pages, packed — packed channels
refused), `page id file`, `char id x y width height xoffset yoffset xadvance page chnl`, `kerning first second amount`.
Pages must be named `<font>_<anything>.png` beside the `.fnt` (they're pipeline-owned, never packed); each page is
decoded, checked against the texture rules, and re-packed by the converter (so BMFont pages also respect D49's limits,
padding and 4-bit coverage). Mono or colour: a page whose every texel is white (any alpha) or opaque grey (grey taken as
coverage) is mono; anything else makes the font a colour font (RGBA8 pages). Metrics: line height = lineHeight, ascent =
base, descent = base − lineHeight; offsets and advances as given; refusals with `file:line: reason` (an id twice, a rect
outside its page, a page missing, values outside int8/u8 ranges after the limits).

**Rasterising** (`ttf_check.*`, `ttf_raster.*`): the source is read whole (≤ 32 MiB), checked by our pre-validator
(table directory, `head`, `hhea`, `maxp`, `hmtx`, `loca`/`glyf` ranges per glyph, `cmap` formats 4/12 bounds, `kern`
format 0 and `GPOS` pair-adjustment lookup bounds, CFF INDEX and charstring bounds; CFF2 and variable-only fonts refused),
copied into a buffer with 64 KiB of zero padding, then given to stb_truetype (`stbtt_InitFont`, `ScaleForMappingEmToPixels`,
`GetCodepointBitmapBox`, `MakeCodepointBitmap` at subpixel shift 0, kern/GPOS advances). Its TU is compiled with
`-ffp-contract=off` (and `/fp:precise` under MSVC); converted fixtures have golden checksums on every host compiler
(AC-2). Coverage → 4-bit as above. Outline glyphs: the coverage dilated by a disk of radius r (max over integer offsets
with `dx² + dy² ≤ r² + r`), drawn under the glyph.

**Packing** (`glyph_pack.*`): glyphs (and outline glyphs) sorted by height, then width, then code point, placed on
shelves left to right; the page side grows 64 → 128 → … → 1024 (or the forced `page`) before a new page is opened; at
most 8 pages, else `fonts/body_ja.font: 6214 glyphs at 16 px need 11 pages of 1024x1024 (limit 8): reduce the size or
the character set`. Deterministic for equal inputs.

**The `.nnhfont` format** (`src/font/font_format.*`, version 1, little-endian, shared with the converter): a 96-byte
header (`NNHF`, version, flags: colour, outline, kerning, antialias; size; line height; ascent; descent; glyph, kerning
and page counts; replacement and ellipsis glyph indexes; fallback count; file size; CRC-32 of the rest; section offsets),
the code point array, 16-byte glyph records (`x, y` u16; `w, h` u8; `ox, oy` i8; `advance` u8; `page` u8; `flags` u8:
break class 3 bits, has outline, has kerning; `kern_count` u8; `kern_first` u16), 8-byte outline records (when flagged),
4-byte kerning pairs, ≤ 3 fallback paths (64 bytes each), the licence path (64 bytes), page records (format A4 or RGBA8,
codec raw or PackBits, width, height, offset, bytes) and page data. The validator checks every offset, size and count,
strictly ascending code points, records inside their page, kerning runs inside the pair array and sorted, page sides and
counts, decoded page sizes exactly (the codec stops at the page's bytes), and the CRC. Fuzzed (`fuzz_font_validate`).

**The font table and loading** (`font_table.*`, `font_load.cpp`, `font_index.cpp`): `load_font(path)` hashes the path;
a loaded path adds a reference and returns another owner of the same slot. Otherwise: `asset_open`, read and validate the
header, allocate the block (header + glyph tables + derived indexes, tag `font`), read the tables, derive the Latin-1 table
and (if needed) the BMP bucket index, then per page: read into a staging allocation (≤ 512 KiB for a 1024² A4 page, freed
at once), decode, create the texture (`detail::texture_from_coverage` in `gfx/texture_alpha.cpp`, tag `font`, or
`texture_create_rgba` for colour pages), then load the fallback paths (one level: a fallback's own fallbacks are
ignored, logged once). Any failure releases what was made. The first successful load sets `detail::g_font_resolve`.
`Font`'s destructor drops a reference; the last frees the block and releases the page textures through gfx (D47).

**Text layout** (`src/text/text_face.hpp`, `text_runs.*`, `text_wrap.*`, `text_draw.cpp`, `text_measure.cpp`): a
*face* is the built-in font (advance `8 × scale`, line `10 × scale`, ascent `8 × scale`, no kerning) or a font block +
its fallbacks + scale. A line's pen starts at 0; each code point: `\r` skipped; spaces (U+0020, U+00A0, U+3000) advance
without a draw; combining marks (Mn/Me ranges of the break table), variation selectors and ZWJ/ZWNJ/ZWSP advance 0 and
draw nothing; any other: glyph from the face (fallbacks, replacement, box), `x += kerning(prev, cur)` (only within one
font; 0 across fonts), draw at `x + ox`, `y + oy`, `x += advance`. Alignment per line: `Center` subtracts `width / 2`
rounded down, `Right` subtracts `width`; position = the game's float `position` + these integer offsets × scale; the line
box is `line_height × scale`. A line's **width** = the pen after its last code point, but at least the last drawn glyph's
`ox + w` (its ink), so measuring covers every drawn pixel (shadow: +scale both ways; outline glyphs: their own boxes,
documented extents). `measure_text(utf8, options)` = `{widest line, lines × line height} × scale`; the old
`measure_text(utf8, scale)` is unchanged. `measure_chars` writes per code point `{byte, x, y, advance, line}` after
alignment (the cursor/typewriter helper).

**Wrapping** (one implementation, moved from `src/ui/ui_wrap.*` to `src/text/text_wrap.*`, now in pixels through a
face): break opportunities, in order — `\n` (mandatory); after a run of spaces (the spaces at a break are dropped); after
`-` (U+002D) and U+2010; **between two characters of class ID** (CJK ideographs U+2E80–U+2FDF, U+3005–U+3007,
U+3021–U+3029, U+3038–U+303B, U+3400–U+4DBF, U+4E00–U+9FFF, U+F900–U+FAFF, U+20000–U+3FFFF; hiragana and katakana
U+3041–U+30FF; fullwidth forms U+FF01–U+FF60, U+FFE0–U+FFE6; halfwidth katakana U+FF65–U+FF9F) — Hangul is not ID (it
breaks at spaces only); a word wider than the line breaks by characters. **Kinsoku** applies only to ID break
opportunities: no line starts with a character of the no-start table (JIS X 4051 classes 2–6, 9–11: `、。，．・：；？！゛゜
ヽヾゝゞ々〻ー‐」』）〕］｝〉》】〙〗〟’”｠…‥〜` and the small kana `ぁぃぅぇぉっゃゅょゎゕゖァィゥェォッャュョヮヵヶ`, their fullwidth
forms `！％），．：；？］｝`, halfwidth `｡｣､･ｰ`), none ends with one of the no-end table (class 1: `「『（〔［｛〈《【〘〖〝‘“｟`
and fullwidth `（［｛`); a violating break moves back one character (oidashi), at most twice, else breaks where it was —
the same on every system. Every table entry is ≥ U+2000, so text of Latin-1 characters wraps exactly as spec 028's
wrapper did (proved below). `max_lines` cuts the last line so that it plus the ellipsis fits (the font's U+2026 if it
has one, else `...`; built-in: `...`), dropping its trailing spaces. Break classes come from `text/break_class.cpp`
(pure, compiled into `nnh-assetc` too); a game font's glyph records carry them precomputed (the converter calls the same
function; a test checks every fixture glyph).

**Missing characters and logs** (`text/text_missing.cpp`, Debug/Development only): a fixed set of 128 `(font slot, code
point)` pairs remembers what was logged (`text: U+4E00 not in fonts/body_16`); when full, one `text: more missing
characters (log full)` line. Release: nothing. The hollow box at a font's size: a 1-pixel `RectOutline` command of
`(max(4, size / 2) − 2) × (ascent − 1)` at `pen + (1, 1)`, × scale, advance `max(4, size / 2)`.

**String files** (`strings_read.*`, `strings_model.hpp`; TOML strings mode): keys `[A-Za-z0-9_-]` parts joined by
dots, ≤ 128 bytes; a table header is a prefix (`[menu]` + `new_game` = `menu.new_game`); values are basic or multi-line
basic strings (UTF-8, ≤ 4 096 bytes after escapes) — anything else is refused with its line. The `[language]` table and
its sub-tables are metadata, never keys: `name` (required, ≤ 63 bytes), `fallback` (a shipped code), `grouping` (≤ 4
bytes of UTF-8, e.g. `","`, `"."`, `" "` (U+00A0), `" "`), `grouping_min` (1..9 digits, default 4), `[language.fonts]`
(role = font asset path, ≤ 8 roles, role names `[a-z0-9_]`), `[language.plural]` (category = CLDR rule text, for a code
the engine doesn't know), `[language.limits]` (base only: key prefix = maximum characters). A language's code comes from
its file or folder name and must be a well-formed tag: 2–3 lowercase letters, optional `-Xxxx` script, optional `-XX` or
`-999` region (`en`, `pt-BR`, `zh-Hant`, `es-419`), ≤ 15 bytes, unique case-insensitively.

**Placeholders and plurals** (`placeholders.*`, `plural_compile.*`): `{name}` with `name` in `[A-Za-z_][A-Za-z0-9_]*`,
≤ 16 per string; `{{` and `}}` are braces; anything else starting with `{` or a lone `}` is malformed. A plural key's
argument is `<key>.plural` if given, else `count` if the base's `other` form uses `{count}`, else the first placeholder of
the base's `other`; a plural key with no placeholder in any form needs `.plural`. CLDR rule text (integer subset): `n`
and `i` (equal for integers), `%` by 10, 100, 1000 or 1000000, `=`/`!=` against values, ranges `a..b` and lists `a,b`,
`and`, `or`. Compiled bytecode ≤ 64 bytes per language (stack-free: each category a disjunction of conjunctions of
`(operand, modulus, negate, ranges[])`). **Shipped rules** (CLDR 46): `ja zh ko vi id ms th` (other); `en de nl sv da nb fi
et it el hu tr bg ca` (one/other; `it`, `ca` also many for 1 000 000…); `es fr pt` (one, many, other; `fr` and `pt`:
one = 0..1); `pt-PT` (one = 1, many, other); `ro` (one, few, other); `ru uk` (one, few, many, other); `pl` (one, few, many,
other); `cs sk` (one, few, other for integers); `hr sr` (one, few, other); `sl` (one, two, few, other); `lt` (one, few,
other); `lv` (zero, one, other). A language without a shipped rule and without `[language.plural]` may have only `other`
forms (error otherwise). **Required categories**: those the rule produces for some integer in 0..1 000 999 or a power of
ten up to 10¹⁸ (computed by the compiler); `other` is always required. `tr(key)` of a plural key returns `other`'s text.

**Build checks** (`strings_check.*`, `coverage.*`; messages exactly, the tests pin them):
- errors: `strings/de.strings:4: invalid UTF-8 (byte 0xC3 at column 18)`; the TOML reader's own messages; `strings/en.strings:9:
  "menu.new_game" is set twice (first at line 4)`; `strings/de.strings:20: "menu.optons" is not in the base language en
  (did you mean "menu.options"?)`; `strings/de.strings:12: "items.count": placeholder {cnt} is not in en's text (en has
  {count})`; `strings/de.strings:12: "hud.score": placeholder {score} missing (en has it)` (non-plural keys; a plural
  key's forms together must use every base placeholder except its argument); `strings/ru.strings:30: "items.count" has no
  "other" form`; `strings/ru.strings:30: "items.count" needs the plural forms one, few, many, other for ru (missing:
  few, many)`; `strings/en.strings:7: "hud.score": unclosed "{" at column 12 (write "{{" for a brace)`;
  `strings/ja.strings:4: font role body names fonts/body_jp, which is not a font (did you mean fonts/body_ja_13?)`;
  `strings/ja.strings:41: "shop.buy" uses U+8CB7 '買' not in font fonts/body_ja_13 (role body) or its fallbacks`;
  `strings/en.strings:88: keys "a.b" and "x.y" (strings/en.strings:12) have the same hash 0x1234ABCD: rename one`;
  `strings/pt-PT.strings:3: fallback "pt-BR" is not a shipped language`; a fallback cycle; a missing base file; a
  language in `LANGUAGES` without files, or files of a language not listed.
- warnings (`STRICT_STRINGS` makes them errors): `strings/de.strings: warning: 12 keys of en have no translation (en's
  text is used): menu.quit, shop.title, … (first 5)`; `strings/ja.strings:33: warning: "dialog.intro" contains U+0301
  (a combining mark draws nothing)`; `strings/de.strings:18: warning: "menu.options" is 21 characters (limit 18 for
  "menu.")`; coverage of `qps` (Debug/Development) by the base fonts.

**Converted tables** (`src/loc/loc_file.*`, shared): `.nnhstr` version 1 — a 64-byte header (`NNHL`, version, flags
(names), code[16], counts, bucket bits, CRC-32, size), bucket index (`2^b + 1` × u16, `2^b ≥ keys / 2`), 12-byte key
records sorted by hash (hash, first string u16, form mask u8, flags u8, plural argument hash), 8-byte string records
(text offset u32, length u16, first placeholder u16), 8-byte placeholder records (name hash, byte offset u16, span length
u8, end flag), the text (each string NUL-terminated; `{{` stored as `{`; placeholders kept as written so `tr` shows
`{name}`), Debug/Development names (offset per key + bytes), and a zero guard. `.nnh_loc` version 1 — header, per
language 96 bytes (code, display name offset, chain of ≤ 4 indexes ending at the base, plural bytecode offset, grouping,
grouping_min, table size, role table offset), role names (Debug/Development) and hashes, font paths, the largest chain's
bytes, the base index, the pseudo index (or none). Both validated whole (bounds, sorted unique hashes, buckets consistent
with hashes, records and spans inside the text, UTF-8 of every string, NUL terminators, bytecode well-formed, chains
acyclic and ending at the base). Fuzzed (`fuzz_loc_table`, `fuzz_loc_manifest`).

**Start and matching** (`loc_state.cpp`, `loc_match.*`): `start(options)` (asserts if started): reads `.nnh_loc` into a
small allocation, allocates the block (largest chain bytes + 16-byte guard), picks the language: `options.language` if
shipped (why: player), else `system::language()` matched (why: system), else base (why: base); matching of a code `c`:
exact (case-insensitive) → a shipped code with the same language and script and any region (`pt` ↔ `pt-BR` ↔ `pt-PT`; the
system code's own region first, then the language alone, then shipped order) → base. Scripts never match each other
(`zh-Hant` doesn't take `zh-Hans` unless a file's `fallback` says so). Loads the chain into the block (tables
concatenated, current first), sets `follow_system`, publishes `LanguageChanged{kNoLanguage, i, 0, serial 1}`, logs once
in Debug/Development `loc: language ja (system)`.

**Lookup** (`loc_lookup.cpp`): for each table of the chain: `b = hash >> (32 − bits)`, scan `[bucket[b], bucket[b+1])`;
found → the string's text. None → Debug/Development: `Key::name` (or `#XXXXXXXX` for a run-time hash) and one log per key
(`loc: unknown key "menu.optons"`, 64 remembered); Release: `"??"`. `show_keys` → the key's name (names table) for every
found key. Before `start()`: assert; Release `""`. `resolve(hash)` = `tr(Key(hash))`.

**Formatting** (`loc_format.cpp`, `loc_number.*`, `plural.*`): pick the string (plural: the argument by name hash, an
integer, through the interpreter on `|n|`; a missing or non-integer argument → `other`, Debug log), copy literal spans,
replace each placeholder by its argument: integer → decimal with the language's grouping (`INT64_MIN` exact; digits by a
constant division), string → its bytes, key → `tr(key)`'s text; an unused argument is ignored; a placeholder without an
argument → `{name}` in Debug/Development (logged once per key) and nothing in Release. Output truncated at the last
whole code point that fits `capacity − 1`, NUL-terminated (capacity 0: returns 0, Debug assert); returns the bytes
written; a truncation logs a Debug/Development warning once per key. No allocation, no `printf`.

**Switching** (`loc_switch.cpp`): `set_language(code)` (before `start()`: `InvalidArgument`, assert) → `NotFound` if not
shipped (Debug log), else records it as pending (a later call in the same frame wins) and adds the after-input hook. At
the next `nnh::frame()`, after input and before audio and dispatch, the hook: reads the new chain into the block (any
failure: reloads the previous chain, logs an error, publishes nothing), zero-fills the rest, swaps role fonts (if fonts
are used: new paths loaded first, then the old ones released — shared paths keep their slot), increments the serial,
publishes `LanguageChanged`, logs `loc: language pt-BR (player)` (Debug/Development), and removes itself unless following
the system language. With `follow_system(true)` the hook stays registered and compares `system::language()` (≤ 16 bytes)
each frame; a change matches and switches as above (why: system). `set_language` while following turns following off.

**Fonts per language** (`loc_fonts.cpp`): `use_fonts()`, `font(role)` and `apply(…)` set the module's font hook; when it
is set, `start()` and every switch load the language's role fonts (a role missing in a language comes from its chain);
`font(role)` called before the hook was set loads the current language's fonts at once (a Debug log suggests
`use_fonts()` before `start()`). An unknown role → `FontRef{}` (built-in) and one Debug/Development log. Peak memory of a
switch = outgoing + incoming fonts for one frame (D47 frees textures after the next `begin_frame`).

**The UI hook** (`loc_ui.cpp`, the only file including `<nnh/ui.hpp>`): `apply(ui, roles, n)` = `ui.set_resolver(&resolve)`,
`ui.set_font(role.slot, font(role.role))` for each, `ui.text_changed()`. The documented pattern: call it in `on_enter()`
and from a `LanguageChanged` subscription owned by the scene.

**Pseudo-language** (`pseudo.*`, converter, Debug/Development only): from each base string: letters mapped to Latin-1
look-alikes (`a→å e→é i→î o→ö u→ü c→ç n→ñ y→ý` and capitals), placeholders and `{{`/`}}` kept, `~` × ⌈0.3 × characters⌉
appended before the closing bracket, the whole wrapped in `[` `]`; plural forms likewise; code `qps`, display name
`[Pséüðö]`, base's roles and grouping. Never generated in Release.

**The system language** (`system/system_language.cpp`, `system/language_override.cpp`; contract `system_language()`):
- **3DS** (`language_3ds.cpp`): `CFGU_GetSystemLanguage` and `CFGU_SecureInfoGetRegion` once (cached): `JP→ja`, `EN→en-US`
  (USA region) / `en-GB` (EUR, AUS) / `en`, `FR→fr-CA` (USA) / `fr-FR` (EUR) / `fr`, `DE→de`, `IT→it`, `ES→es-419` (USA) /
  `es-ES` (EUR) / `es`, `ZH→zh-Hans`, `KO→ko`, `NL→nl`, `PT→pt-BR` (USA) / `pt-PT` (EUR) / `pt`, `RU→ru`, `TW→zh-Hant`;
  a failed service call → `en`. The maintainer's console checks one row (AC-4); the table is from libctru's `cfgu.h`.
- **PC** (`language_pc.cpp`): `NNH_SIM_LANGUAGE` if it is a well-formed tag, else `language=` in `language.ini` in the
  game's simulator data folder (beside `window.ini`, read once, bounded), else `en`. No hotkey in this spec (decision 14).
- **fake** (`language_fake.cpp`): `en`, set by tests with `fake::set_system_language()`.
- **scripts**: a `Language` message overrides it until `language off` or the script ends; an exclusive (script) Hello
  sets it to `en` first, as it sets the slider to 0, so scripts give the same lines on any console.

**Misuse (FR-35)**, assert in Debug/Development and harmless in Release: a stale `FontRef` (built-in font), a scale
outside 1–8 (clamped, as spec 005), an `outline_color` on a font without outlines (ignored, logged once), `tr`/`format`
before `start()` (`""`), a format capacity ≤ 0 (returns 0), an unknown role (built-in font), `set_language` of a code not
shipped (`NotFound`, no change), `start()` twice (ignored), more than 32 fonts (`Unsupported`), `Ui::set_font` slot
outside 0..7 (ignored), a `Font` destroyed while the UI or localization still names it (stale ref → built-in). Never
undefined behaviour (ASan/UBSan, the random-operation test).

### Spec 028's seam: from cells to advances

What changes inside `src/ui/` and why nothing visible changes with the built-in font:

1. **API**: every `Ui` function and desc stays as it is; additions only (`UiFont::Slot0..Slot7`, `Ui::set_font`,
   `kMaxUiFonts`). `Ui::text_cells(label)` keeps counting characters.
2. **The seam** (`ui_text_seam.*`): `TextSize{cells, lines, total}` becomes `{width, lines, total}` in pixels; for
   `UiFont::Builtin` it is computed by the same loop as today and `width = cells × kFontCellSize × scale` once at the end
   (the same integers the layout multiplied before); `seam_cell` disappears from widget code, replaced by
   `seam_width(face, utf8, n)`; for a slot it walks the shared run code through the font's face.
3. **Wrapping**: `src/ui/ui_wrap.*` moves to `src/text/text_wrap.*` and measures in pixels through a face. For the
   built-in face every advance is the cell, the ellipsis is three cells, and every new rule (ID breaks, kinsoku,
   U+2010, U+3000) is keyed to code points ≥ U+2000 that the built-in font draws as the hollow box — so for Latin-1 text
   (all of spec 028's goldens and tests) the break positions are the old ones. Old `max_cells` arguments become
   `cells × cell` pixels at every call site.
4. **Drawing** (`ui_draw_out.cpp`): runs still go to `gfx::draw_text` with the same options; the font slot's `FontRef`
   is added only for a slot, so built-in runs take spec 005's untouched path. The draw hash folds the font slot **only
   when it isn't built-in**, so the built-in hash sequence is unchanged; the glyph count for a slot comes from the face
   (glyphs with a bitmap), for the built-in font from today's inline count.
5. **Block**: the header gains 8 `FontRef`s (32 B); `UiStats::block_bytes` and the documented formula grow by 32 B
   (spec 028's `test_create.cpp` formula updated — the only expected value that changes, recorded).

Proof, at T2.6/T3.5 and again at stage 8 and on the console: (a) every spec 028 test passes with **no expected value
edited** except the block formula (the diff of `tests/ui/` is reviewed: call signatures of the moved wrapper only);
(b) `tests/ui/test_ui_seam_equivalence.cpp`: a test-only copy of the pre-029 wrapper and cell measure
(`tests/ui/reference/ui_wrap_cells.*`) against the new code on 100 000 seeded Latin-1 strings × widths 1..60 cells × max
lines 0..4 — identical lines, bytes, cells × cell and ellipsis flags; (c) `ui_self_check_pc` keeps `30400DA6` and its six
checkpoints; the micro-benchmark draw hash `7429F9EB` (plan 028's `build/ref-028/micro`, rerun as
`build/ref-029/micro_ui`) is unchanged and its relayout time within ±5 % of T0.1's; (d) `examples/ui` screenshots
byte-identical (`compare.sh`); (e) stage 10 run 2: `nnh run ui --script examples/ui/tests/check.nnhs` → `ui: PASS
30400DA6` and the UI bench within its budgets (relayout < 0.93 ms, draw ratio ≤ 1.15).

### Opt-in and `hello`

`hello` links the debug pass (which calls spec 005's `layout_text` template directly), the font texture, gfx's frame,
draw list and `draw_2d`, and on each platform `video`, `draw2d`, `texture`, `system` and `window` sources. None of these
files changes:

- the new contract functions live in **new files** per platform (`texture_alpha_<p>.cpp`, `language_<p>.cpp`), linked
  only when core's `gfx/texture_alpha.cpp` or `system/system_language.cpp` is; the 3DS's `Texture3ds` struct moves from
  `texture_3ds.cpp`'s anonymous namespace to `texture_3ds.hpp` (same layout; the object file is compared at T1.3);
- `draw_text.cpp` changes (the dispatch to the new path) but `hello` doesn't link it; `layout_impl.hpp`, `utf8.cpp`,
  `font_texture.cpp`, `debug_pass.cpp`, `gfx_frame.cpp`, `draw_list.cpp` are untouched;
- the simulator's language comes from its own file (`language.ini`) read by `language_pc.cpp`, so `window_pc.cpp` and
  `sim_settings.cpp` don't change;
- the font module is reached only through `g_font_resolve` (set by `load_font`), the localization module only through
  `loc::` calls; no static initialiser exists in either.

**Debug/Development flavours** link the remote session, whose decoder gains the `Language` message and whose session
calls `detail::set_language_override` (`language_override.cpp`: 16 bytes and a serial): 3ds-dev and pc-debug `hello` grow
by ≈ 150–250 B, as spec 026b's `Slider` message did (question 5). Checks (T0.1, T1.7, T8.3): 3ds-release and pc-release
`hello` sizes, `nm` and (3DS) bytes identical to T0.1's; Debug/Development growth measured and recorded; probes linked
against the 3ds-release libraries: a font-only game has no `loc::` symbol, a strings-only game (built-in font, no role
fonts) has no `load_font`, `stb`, `font_` symbol, a UI game without `set_font` has no `ui_fonts.o`.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/gfx.hpp` | changed | `FontRef`, four `TextOptions` fields, `FontMetrics`, `TextCaret`, `measure_text(utf8, options)`, `measure_chars`, `font_metrics` |
| `engine/core/include/nnh/font.hpp` | new | `Font`, `load_font`, limits, `font::debug_draw_pages` (≈ 110 lines) |
| `engine/core/include/nnh/localization.hpp` | new | `LanguageChanged`, `loc::` API above (≈ 190 lines) |
| `engine/core/include/nnh/system.hpp` | changed | `system::language()` |
| `engine/core/include/nnh/ui.hpp`, `detail/ui_style.hpp` | changed | `UiFont` slots, `kMaxUiFonts`, `Ui::set_font` |
| `engine/core/contract/nnh/platform/video.hpp` | changed | `texture_create_alpha` |
| `engine/core/contract/nnh/platform/system.hpp` | changed | `system_language()` |
| `engine/core/src/text/break_class.{hpp,cpp}` | new | line-break classes and kinsoku tables (pure; also in `nnh-assetc`) |
| `engine/core/src/text/text_face.hpp` | new | built-in or font face: glyph, advance, kerning, class (inline over `font_block.hpp`) |
| `engine/core/src/text/text_runs.{hpp,cpp}` | new | a line's pen walk: kerning, ink extent, alignment, scale (pure) |
| `engine/core/src/text/text_wrap.{hpp,cpp}` | new (moved) | the shared wrapper (spec 028's rules + ID breaks, kinsoku, ellipsis) in pixels |
| `engine/core/src/text/text_measure.cpp` | new | `measure_text(utf8, options)`, `measure_chars`, `font_metrics` |
| `engine/core/src/text/text_draw.cpp` | new | the font/wrap/outline path of `draw_text`: commands, shadow, outline, fallbacks, replacement, box |
| `engine/core/src/text/text_missing.{hpp,cpp}` | new | missing code points logged once (Debug/Development) |
| `engine/core/src/gfx/draw_text.cpp` | changed | dispatch to `text_draw.cpp`; the built-in path untouched |
| `engine/core/src/gfx/texture_alpha.cpp` | new | `detail::texture_from_coverage` (table slot, tag, D47 release) |
| `engine/core/src/font/font_format.{hpp,cpp}` | new | `.nnhfont` v1 layout and writer helpers (shared with `nnh-assetc`) |
| `engine/core/src/font/font_validate.cpp` | new | bounded validator (shared, fuzzed) |
| `engine/core/src/font/page_codec.{hpp,cpp}` | new | PackBits encode/decode of pages, A4 nibble unpack (shared, bounded) |
| `engine/core/src/font/font_block.hpp` | new | the run-time block (header, arrays, derived indexes), inline lookups |
| `engine/core/src/font/font_index.cpp` | new | Latin-1 table and BMP buckets derived at load; lookups beyond the BMP; kerning search |
| `engine/core/src/font/font_table.{hpp,cpp}` | new | 32 entries, refcount by path hash, `FontRef` resolve, `g_font_resolve` |
| `engine/core/src/font/font_load.cpp` | new | `load_font`: read, validate, block, pages, fallbacks, failure cleanup |
| `engine/core/src/font/font.cpp` | new | `Font` (move, release, metrics, has, covers, advance, kerning, path), `FontRef(const Font&)` |
| `engine/core/src/font/font_debug.cpp` | new | `dump`, `debug_draw_pages` (Release: empty) |
| `engine/core/src/loc/loc_file.{hpp,cpp}` | new | `.nnhstr` / `.nnh_loc` v1 layouts (shared with `nnh-assetc`) |
| `engine/core/src/loc/loc_validate.cpp` | new | table and manifest validators (shared, fuzzed) |
| `engine/core/src/loc/plural.{hpp,cpp}` | new | the plural bytecode interpreter (shared with the compiler's tests) |
| `engine/core/src/loc/loc_state.{hpp,cpp}` | new | module state, `start`, `stop`, the block, shutdown hook |
| `engine/core/src/loc/loc_match.{hpp,cpp}` | new | tag parsing and matching (pure) |
| `engine/core/src/loc/loc_lookup.cpp` | new | `tr`, `resolve`, chains, markers, `key()` |
| `engine/core/src/loc/loc_format.cpp` | new | `format`, arguments, placeholders, plural choice, truncation |
| `engine/core/src/loc/loc_number.{hpp,cpp}` | new | 64-bit integers with grouping (pure) |
| `engine/core/src/loc/loc_switch.cpp` | new | pending switch, the after-input hook, `LanguageChanged`, serial, following |
| `engine/core/src/loc/loc_queries.cpp` | new | code, name, index, list, `find` |
| `engine/core/src/loc/loc_fonts.cpp` | new | `use_fonts`, `font(role)`, role loading and sharing |
| `engine/core/src/loc/loc_ui.cpp` | new | `apply(Ui&, …)` (the only file including `<nnh/ui.hpp>`) |
| `engine/core/src/loc/loc_debug.cpp` | new | `dump`, show keys, unknown-key log (Release: empty) |
| `engine/core/src/system/system_language.cpp`, `language_override.cpp`, `language_tag.{hpp,cpp}` | new | `system::language()`; the remote override; tag well-formedness (pure, no engine includes: also compiled into the PC platform library and `nnh`, like `remote_wire.cpp`) |
| `engine/core/src/remote/remote_wire.{hpp,cpp}`, `remote_session.cpp` | changed | `MessageType::Language` (8), packing into `press`/`release`, apply/off/exclusive `en` |
| `engine/core/src/ui/ui_text_seam.*`, `ui_layout_measure.cpp`, `ui_draw_widgets.cpp`, `ui_draw_list.cpp`, `ui_reveal.cpp`, `ui_list.cpp`, `ui_parts.cpp`, `ui_draw_out.*`, `ui_block.hpp`, `ui_create.cpp` | changed | pixels instead of cells; font slots in the header; hash rule above |
| `engine/core/src/ui/ui_wrap.{hpp,cpp}` | removed | moved to `src/text/text_wrap.*` |
| `engine/core/src/ui/ui_fonts.cpp` | new | `Ui::set_font` |
| `engine/core/CMakeLists.txt` | changed | `_nnh_text_sources` additions, `_nnh_font_sources`, `_nnh_loc_sources` (`-ffp-contract=off` not needed: no float) |
| `engine/platforms/pc/src/texture_alpha_pc.cpp`, `language_pc.cpp` | new | white RGBA8 + coverage, bytes = w × h; `NNH_SIM_LANGUAGE`, `language.ini` |
| `engine/platforms/3ds/src/texture_alpha_3ds.cpp`, `texture_3ds.hpp`, `language_3ds.cpp` | new | GPU_LA4 (`0xF0 | a4`) Morton-swizzled; the shared `Texture3ds`; CFGU language + region |
| `engine/platforms/3ds/src/texture_3ds.cpp` | changed | `Texture3ds` from the header (no code change) |
| `engine/platforms/fake/src/texture_alpha_fake.cpp`, `language_fake.cpp`, `include/nnh/platform_fake.hpp` | new / changed | copies and counters (`alpha_textures_created`), `set_system_language` |
| `engine/platforms/*/CMakeLists.txt` | changed | the new sources |
| `third_party/stb_truetype/{stb_truetype.h, stb_truetype.c, README.md, CMakeLists.txt}` | new | pinned, unmodified; a host-only object library (`-ffp-contract=off`, warnings off) |
| `third_party/stb_image/{stb_image.h, stb_image.c, README.md, CMakeLists.txt}` | new (question 2) | PNG only (`STBI_ONLY_PNG`, `STBI_NO_STDIO`), host-only |
| `tools/assetc/src/font_desc.*`, `charsets.*`, `bmfont_text.*`, `bmfont_convert.*`, `png_pages.*`, `ttf_check.*`, `ttf_raster.*`, `outline_bake.*`, `glyph_pack.*`, `font_writer.*`, `convert_font.*`, `cli_font.cpp` | new | the font converter (`nnh-assetc font`) |
| `tools/assetc/src/strings_read.*`, `strings_model.hpp`, `placeholders.*`, `plural_rules_cldr.*`, `plural_compile.*`, `strings_check.*`, `strings_writer.*`, `coverage.*`, `pseudo.*`, `manifest_writer.*`, `cli_strings.cpp`, `cli_strings_index.cpp` | new | the string converter (`nnh-assetc strings`, `strings-index`) |
| `tools/assetc/src/toml*.{hpp,cpp}` | changed | a strings mode (dotted keys, multi-line basic strings, 1 MiB/4 KiB limits); data mode unchanged |
| `tools/assetc/CMakeLists.txt`, `tools/assetc/src/cli.cpp`, `main.cpp` | changed | the new subcommands and libraries |
| `cmake/nnh_assets.cmake`, `cmake/nnh_game.cmake`, `cmake/nnh_strings.cmake` (new) | changed / new | routing (`.font`, `.fnt` + pages, `.ttf/.otf/.ttc/.chars` not packed, `strings/`), `LANGUAGES`/`BASE`/`STRICT_STRINGS`, per-language and index steps |
| `tools/nnh/src/remote/script_parser.cpp`, `script_timeline.*`, `script.hpp` | changed | `language CODE|off` |
| `scripts/make_test_fonts.py` | new | writes the TTF and BMFont fixtures (stdlib only; outputs committed) |
| `scripts/check_release_strings.sh` | changed | the new Debug/Development texts |
| `tests/text/`, `tests/font/`, `tests/loc/`, `tests/ui/` (+ `reference/`), `tests/system/`, `tests/remote/`, `tests/platform_conformance/`, `tests/fuzz/`, `tools/assetc/tests/`, `tools/nnh/tests/` | new / changed | the test strategy below |
| `tests/data/fonts/`, `tests/data/strings/` | new | fixtures (generated fonts, descriptions, BMFont files, string files and error cases) |
| `tests/loc_example/test_self_check.cpp` | new | AC-2 golden |
| `tests/CMakeLists.txt` | changed | one 029 block; fixture conversion at build; ctests `font_fixtures_golden`, `loc_self_check_pc`, `loc_check_script_pc` |
| `examples/localization/` | new | the example (below) |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(localization)` after `ui` |
| docs (stage 11) | new / changed | see tasks T11.1–T11.7 |

### Data and memory

| Item | 3DS | 64-bit PC | Notes |
|------|-----|-----------|-------|
| glyph | 20 B (+8 with an outline) | same | code point + record |
| kerning pair | 4 B | same | per left glyph run |
| font block overhead | ≈ 0.7 KB (Latin) / ≈ 4.8 KB (with glyphs > U+00FF) | same | header 96 B, Latin-1 table 512 B, BMP buckets 4 KB, paths |
| font table | 512 B `.bss` | 768 B | linked only with fonts |
| page | w × h bytes (LA4) | w × h × 4 in the GPU, **accounted w × h** | the simulator enforces the console's numbers (D4) |
| staging for a page | ≤ 512 KiB, freed at once | same | 4-bit data of a 1024² page |
| string key | 12 B + ≈ 1 B of buckets | same | + 8 B per string (form), 8 B per placeholder, text + NUL |
| key names (Debug/Development) | 4 B + the name | same | not in Release |
| manifest | 96 B per language + names + rules + roles (≈ 4 KB for 6 languages) | same | tag `strings` |
| loc state | ≈ 0.4 KB `.bss` (+ 0.3 KB Debug logs) | ≈ 0.5 KB | linked only with `loc::` |
| `Ui` block | +32 B (font slots) | +32 B | every UI |

The example (estimates; measured at T7.5 and logged by the bench): HUD BMFont (≈ 110 glyphs, one 128 × 64 A4 page) 10 KB;
Latin body 16 px (ASCII, Latin-1, Cyrillic, language names: ≈ 420 glyphs, 512 × 512) 265 KB; title 32 px (≈ 190 glyphs,
512 × 512) 260 KB; Japanese body 13 px (example strings + kyōiku + kana + ASCII: ≈ 1 400 glyphs, 1024 × 512) 545 KB;
Japanese title 32 px (≈ 160 glyphs, 512 × 512) 260 KB; tables: `en` 64 KiB (it carries the generated bench section) and
≈ 6–10 KB for each other language, block ≈ 75 KB. Steady state ≈ 0.6 MiB in English, ≈ 0.9 MiB in Japanese; peak during
an en ↔ ja switch ≈ 1.45 MiB (both languages' fonts for one frame) — **under AC-6's 2 MiB**. The Japanese body page is one
1024 × 512 page (≤ 1 MiB): AC-6's "one 1024 × 1024 page" holds.

### Platform work

- **pc:** `texture_alpha_pc.cpp` (expand to white RGBA32 in a temporary buffer, `SDL_CreateTexture` + `SDL_UpdateTexture`,
  nearest, blend; `bytes = w × h`); `language_pc.cpp` (env, `language.ini`, `en`). Conformance + a pixel test
  (`test_texture_alpha_pc.cpp`: a coverage texture tinted red over blue equals an RGBA white-alpha texture drawn the same
  way, byte for byte, read back through `remote_capture`).
- **3ds:** `texture_alpha_3ds.cpp` (`C3D_TexInit(GPU_LA4)`, bytes `0xF0 | (a / 17)` Morton-swizzled by 8 × 8 tiles with a
  64-entry offset table, `C3D_TexFlush`, nearest, clamp) — the LA4 nibble order (luminance high, alpha low) and the 4 → 8
  bit expansion (`v × 17`) are taken from 3dbrew's texture format table and checked on the console (AC-5);
  `language_3ds.cpp` (`cfguInit`, the two queries, `cfguExit`, the table above); `texture_3ds.hpp`.
- **fake (tests):** `texture_alpha_fake.cpp` keeps the coverage bytes per handle and counts creations;
  `language_fake.cpp` returns the value tests set; the fake's 2D items already record texture, region, position, colour.

### Build pipeline and build budgets

- **Routing** (`nnh_assets.cmake`, before the data rule, like D31/D42): `*.font` → `font` step → `<path>.nnhfont`;
  `*.fnt` → `font` step → `<path>.nnhfont`, and `<stem>_<any>.png` beside it are pipeline-owned; `*.ttf`, `*.otf`, `*.ttc`,
  `*.chars` not packed; `strings/<code>.strings` and `strings/<code>/*.strings` grouped per language → `strings` step →
  `strings/<code>.nnhstr`, then one `strings-index` step → `.nnh_loc` (+ `strings/qps.nnhstr` in Debug/Development);
  `.strings` outside `strings/` → configure error. `font`, `fnt`, `ttf`, `otf`, `ttc`, `chars`, `strings`, `nnhfont`,
  `nnhstr` are not data keys (D29's routing, as D31 did). `nnh-assetc` is built when any of these files exist.
- **Dependencies**: a font step's depfile lists its description, source font, `.chars` files and the string files of
  its `strings` languages; a language step depends on its files and the base's; the index step on every table, every
  `.nnhfont`, the language files' sources (for line numbers) and the language list. Editing `ja.strings` reruns the `ja`
  step, the fonts that list `ja`, and the index (target < 2 s for the example, T8.4). Both tools rewrite an output only
  when its bytes change.
- **Flavour**: Release presets pass `--release` (no names, no `qps`); `LANGUAGES` and `BASE` reach the index step through
  a generated list file (rewritten only when it changes).
- **Budgets**: the converters are never on the "edit one game `.cpp`" path; `<nnh/font.hpp>` and `<nnh/localization.hpp>`
  measured (T3.8, T6.8); clean `nnh_core` +≈ 30 sources (plan 028's 16.0–16.3 s → expected ≈ 19 s, < 30 s; T0.1 re-measures); `nnh-assetc`'s
  clean build grows by stb_truetype and stb_image (≈ 3–4 s on the host; only built for games with fonts, maps, sprites,
  models or data, as today).

### ARM11 cost plan (AC-6)

Calibration from specs 019–028 on the Old 3DS (ARM11 268 MHz, VFPv2, 16 KB L1D, 32-byte lines, ≈ 110 cycles per miss):
≈ 2 cycles per instruction in practice, and **plan 028's lessons**: its relayout estimate (0.3–0.58 ms) measured 1.48 ms
before and 0.744 ms after an optimisation round (2–5× optimistic), and its list bench counted the next frame's untimed
drawing. So: (1) every estimate below gives a range and the upper end × 1.3, and anything within 1.5× of its target is
marked **at risk**; (2) every bench phase times exactly its workload — drawing phases measure their own glyphs per
glyph (scope + `gfx submit` delta over a base run with no text), non-drawing phases (`measure`, `tr`, `format`, `switch`)
draw nothing in the frames they time, and the next frame's submit is never attributed to them.

Built in: Latin-1 direct table and BMP buckets (no binary search for the example's characters); break classes in the
glyph record (no table search while wrapping a game font); kerning only when the left glyph's flag is set (a run of
≤ 255 pairs, short in practice); `tr` = bucket + 1–2 compares on 12-byte records; plural moduli by constant division
(`umull`, no `__aeabi_*`); digits by constant division; literal spans copied with `memcpy`; no float anywhere in
fonts/loc but the final `float` position handed to gfx.

| AC-6 workload | Hot path | Estimate (upper × 1.3) | Target | Verdict |
|---------------|----------|------------------------|--------|---------|
| 400 glyphs of the body font with kerning vs 400 built-in glyphs, CPU per glyph | built-in ≈ 11.5–12.8 µs; extra per game glyph: decode 3, Latin-1 read 4, record 4, kerning flag 2 (+ ≈ 12 on a pair), placement 10, integer → float 4 ≈ 40–60 instructions ≈ 0.3–0.45 µs | ratio 1.02–1.04 (1.05) | ≤ 1.15 | expected PASS |
| 400 glyphs + 300 sprites, 600 frames | ≈ 700 draws × 12 µs ≈ 8.4 ms + base ≈ 1.5 ms + loc/text work < 0.3 ms | worst 10–12 ms (15.6) | no missed frame | expected PASS |
| `measure_text` + wrap of a 400-character Latin paragraph (body font, 300 px) | per character: decode 3, lookup 4, advance 3, kerning 2–14, class 2, wrap state 10–14 ≈ 25–40 instructions | 0.075–0.12 ms (0.16) | < 0.1 ms | **at risk** |
| the same, Japanese (400 characters, mostly kana and kanji) | per character: decode 12, BMP bucket + 1–3 compares 10–16, record 4, class 2, kinsoku look-ahead 3, wrap 10 ≈ 40–50 instructions | 0.12–0.15 ms (0.2) | < 0.15 ms | **at risk** |
| `tr` × 1000 (the example's keys, warm) | call, state load, bucket (shift + 2 loads), 1–2 compares, text pointer ≈ 18–24 instructions with 4 dependent loads ≈ 35–55 cycles | 0.13–0.2 ms (0.27) | < 0.1 ms | **likely missed** |
| `format` × 100 (two placeholders and a plural, ru and en) | lookup ≈ 50 cycles, plural interpreter 40–150, literal copy ≈ 25 bytes, integer + grouping ≈ 30–60, string arg ≈ 30, argument match ≈ 20 ≈ 250–400 cycles | 0.09–0.15 ms (0.2) | < 0.1 ms | **at risk** |
| switch en ↔ ja (the 64 KiB en table + the ja table, ja role fonts incl. a 1024 × 512 A4 page; `text_changed()` on the menu UIs) | romfs reads ≈ 70 KB tables + ≈ 120 KB of page data at an unmeasured rate (est. 5–15 MB/s plus ≈ 2–8 ms per open: 15–50 ms), page decode + LA4 swizzle 512 K px × 6–10 cycles (12–20 ms), glyph tables, release, two relayouts (≈ 1.5 ms) | 35–80 ms (104) | < 100 ms | **at risk** (romfs throughput never measured) |
| memory | the table above | ≈ 1.45 MiB peak | < 2 MiB; JP body in one ≤ 1024² page | expected PASS |

The targets stay as the spec wrote them. T7.7 checks the disassembly against this table before the console (no
`__aeabi_*` divisions in `tr`, the plural interpreter, digits or wrapping; no VFP in fonts/loc except the final
position; `pld` where noted; `-fstack-usage` < 1 KiB on each path). T10.2 measures; a missed target is optimised first
(risk 1's list; no glyph item, string, golden or draw hash may change); then its budget is set from the measurement with
≈ 25 % margin and written in the spec's AC-6, `bench_scene.cpp` and `docs/api/localization.md` / `fonts.md`.

### Self-check and bench (AC-2, AC-6)

- **Converted fixture goldens** (`font_fixtures_golden` ctest): the CRC-32 of every `.nnhfont`/`.nnhstr`/`.nnh_loc` the
  build makes from `tests/data/fonts` and `tests/data/strings` (TTF at 8, 12, 16, 32 px; AA on/off; outline 1 and 3; a
  BMFont mono and a colour one; strings of five languages with the pseudo-language) — equal on WinLibs GCC, GCC 13/14,
  clang 18 and the 32-bit host build (T9.1).
- **Self-check** (`examples/localization/self_check.{hpp,cpp}`, pure over the loaded example assets, compiled into the
  example and into `nnh_tests` through the example's converted assets): for each of en, pt-BR, de, ru, ja (and never `qps`,
  so pc-release agrees): `set_language` + one frame; `tr` of every key of a fixed list (folded bytes); `format` of the shop
  line for counts 0, 1, 2, 5, 21, 1 000, −1, `INT32_MAX`, `INT64_MIN` and a name argument; layout items (through the same
  pure layout function `draw_text` uses: page index, region, x, y, colour) of the dialog paragraph wrapped at 300 and 200 px
  with shadow, with outline, with `max_lines = 3` and the ellipsis, centred and right-aligned, at scale 1 and 2, with
  zoom 2; `measure_text` and `measure_chars` of each; the font metrics and the CRC of each loaded asset. A running FNV-1a
  per language is a checkpoint; the final value is `kLocCheckGolden`. pc-debug records them; pc-release, GCC 13/14,
  clang 18, the 32-bit build and the console must agree. The test also compares the fake's recorded items with the
  pure function's for the same calls.
- **Bench** (`examples/localization/bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}`; 300 frames per phase, average and worst,
  only the measured calls in the profile scope): `base` (no text: the submit baseline), `builtin400` and `font400`
  (400 glyphs, same text, built-in vs body font with kerning: cost per glyph = scope + submit − base, over the glyphs the
  phase drew), `frame` (font400 + 300 16×16 sprites, 600 frames, missed = `time::delta()` > 20 ms), `measure_latin`,
  `measure_ja` (400-character paragraphs, wrap 300 px, nothing drawn), `tr1000`, `format100` (en and ru alternately),
  `switch` (en → ja → en, 10 times each way, 30 frames apart, timed inside the hook; breakdown: tables read, fonts read,
  page decode + upload, release, `text_changed`), `page1024` (`load_font("fonts/bench_1024")` + release: one full
  1024 × 1024 page, the spec's reference), `memory` (bytes per glyph, key, font, the example's totals, peak). Lines:
  `loc bench: <name> avg X ms worst Y ms (target < Z ms) PASS|FAIL`, `loc bench: glyphs builtin B us font F us per glyph
  ratio R (target <= 1.15) PASS|FAIL`, `loc bench: frame worst W ms missed M of 600 PASS|FAIL`, `loc bench: switch avg …
  read … decode … relayout …`, `loc bench: memory glyph 20 B key 12 B fonts … tables … peak …`, then `loc bench: PASS` or
  `loc bench: FAIL (<names>)`; a Debug build logs `not judged (Debug)`.

### The example `examples/localization` (AC-3, AC-5)

Assets: `fonts/hud.fnt` + `hud_0.png` (an original 8 px pixel font with ASCII, Latin-1 and Cyrillic, generated by
`art/make_hud_font.py`, committed), `fonts/body_16.font`, `fonts/title_32.font`, `fonts/body_ja_13.font`,
`fonts/title_ja_32.font`, `fonts/names_13.font` (`chars = ["language-names"]`, the language menu's fallback),
`fonts/bench_1024.font`, `fonts/src/NNHExampleSansJP.otf` (the OFL subset, question 3; with `fonts/src/README.md` outside
the assets: upstream URL, version, SHA-256, the `pyftsubset` command and the character lists), `fonts/src/kyoiku_1006.chars`,
`licences/OFL-NNHExampleSansJP.txt` (packed), `strings/en/main.strings` + `strings/en/bench.strings` (the folder form; the
bench section generated by `tools/make_bench_strings.py` to 64 KiB, committed), `strings/pt-BR.strings`, `de.strings`,
`ru.strings`, `ja.strings`, `art/` (a few sprites for the frame bench). `nnh_add_game(localization ... ASSETS assets
LANGUAGES en pt-BR de ru ja BASE en)`, `AppConfig::gfx = {.max_draws = 4096, .max_textures = 64}`.

Scenes (each owning a spec 028 `Ui` bound to it, `loc::apply(ui_, kRoles)` in `on_enter()` and on `LanguageChanged`):
title (New game, Continue (disabled), Shop, Dialog, Fonts, Options, Quit; the game's title in the title font), options
(a Language choice listing every shipped language by its own name — names drawn through `fonts/names_13` as the body
font's fallback — plus "System language" and, in Debug, the pseudo-language; switching calls `set_language` or
`follow_system(true)`), dialog (a 9-slice-free panel with wrapped, typewriter-revealed text in each language, A shows all),
shop (the plural line `0/1/2/5/21 items`, a price with grouping, a player name argument), font gallery (each font's sample,
metrics and `debug_draw_pages`), HUD (BMFont score and coins with grouping). SELECT cycles the shipped languages; Y the
self-check; X the bench; START quits. Starts with `loc::start({.follow_system = true})` and `use_fonts()`.

Log lines: `loc: showcase`, `loc: language <code> (system|player|base)`, `loc: focus <name>`, `loc: shop <count> <text>`
(the formatted line, ASCII-safe codes for non-Latin: the text's bytes in hex after `ja`/`ru`), `loc: dialog shown n/m`,
`loc: fonts <name> glyphs <n> pages <p>`. Screenshots by `LOC_SHOT=title|options|dialog|shop|gallery|ja|ru|qps|pages`.

### The console script `examples/localization/tests/check.nnhs` (AC-4)

`fail-on "] E "`, `fail-on "loc: FAIL"`, `fail-on "loc bench: FAIL"`; `wait 30`; `expect "loc: showcase" 60`; `wait 10`;
`language pt-BR` → `expect "loc: language pt-BR (system)"`; `tap Y` → the five checkpoint lines and `loc: PASS <golden>`
(windows 300); then each `expect` followed by `wait 10` before the next input: `language de` → the line, `screenshot
title`; navigate to Shop, `screenshot shop`, expect the five `loc: shop` lines; `language ru`, `screenshot ru`; `language
ja`, `screenshot ja`, to Dialog, `wait 120`, `screenshot dialog`, `tap A` → `loc: dialog shown …`; `tap B`; Options →
Language → `tap RIGHT` → `loc: language en (player)`; `tap SELECT` ×n through every language; Fonts → `screenshot gallery`;
`language off`; `tap X` → every bench line and `loc bench: PASS`; `end`. Exact lines, coordinates and counts are recorded
from the `--pc` run at T7.6 into "Stage 10: console run (prepared)".

## Test strategy

All core tests run in `nnh_tests` on the fake platform (Windows; VM 120: GCC 13/14, clang 18 with ASan/UBSan, the
32-bit host build); converter tests in `nnh_assetc_tests`. Expected pen positions, breaks, strings, plural categories,
messages and glyph items are written by hand in the tests (or come from CLDR's published samples), never produced by the
code under test. Fonts in core tests are built in memory by the shared writer (`font_format.cpp`) from hand-made glyph
tables, so core tests don't depend on the rasteriser; converted fixtures are tested and checksummed separately.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tools/assetc/tests/test_font_desc.cpp` (every key, defaults, refusals with `file:line`), `test_bmfont_text.cpp` and `test_bmfont_xml.cpp` (every field, binary refused, malformed lines), `test_bmfont_convert.cpp` (pages named `<font>_N.png`, mono vs colour detection, offsets, kerning), `test_ttf_raster.cpp` (glyf and CFF fixtures, sizes, AA on/off, threshold), `test_cli_font.cpp` (licence required / `own`, the build's report line), CMake routing ctest (`font_routing`: sources not packed, the licence packed) |
| FR-2 | `test_glyph_pack.cpp` (determinism, sides 64..1024, forced page, padding between every pair of rects, the 8-page refusal message exactly), `test_page_codec.cpp` (PackBits round trips, nibble unpack, bounds), `test_font_validate.cpp`, conformance `texture_create_alpha` (bytes = w × h on fake and PC), `test_font_load.cpp` (A4 pages via `texture_create_alpha`, colour via RGBA, tag `font`) |
| FR-3 | `test_charsets.cpp` (every named set's size and first/last code points, `U+` ranges, `chars_from`, `strings` languages, `language-names`), `test_cli_font.cpp` (skipped members noted with counts) |
| FR-4 | `test_font_desc.cpp` (one size per description, 6..128), `test_ttf_raster.cpp` (variable fonts at their default instance; CFF2 refused) |
| FR-5 | `tests/font/test_font_load.cpp` (`NotFound`, `InvalidData` per validator reason, `OutOfMemory` via a failing tag, `Unsupported` at 33 fonts; a path loaded twice shares a slot; fallbacks loaded and released; move; destroy releases textures after the GPU (D47 counters)), `test_font_cost.cpp` (no allocation after load) |
| FR-6 | `tests/font/test_font_metrics.cpp` (line height, ascent, descent, `has`, `covers`, `advance`, `kerning` for hand-built fonts; the built-in's {8, 10, 8, 0, no kerning}); `test_ttf_raster.cpp` (the rounding rule on hand-computed units) |
| FR-7 | `tests/text/test_text_draw_fonts.cpp` (items: page, region, pen + offset, kerning, colour, scale, layer, `ignore_camera`, zoom 2 and 0.5, alignment, `\n`, `\r`, spaces cost nothing, shadow order), `tests/text/test_text_builtin_unchanged.cpp` (built-in items with and without the new options at defaults identical to spec 005's path; a huge `wrap_width` gives the same items) |
| FR-8 | `tests/text/test_text_measure_box.cpp` (every drawn item inside the measured box (shadow and outline extents documented) over 10 000 seeded strings × fonts × scales; ink extending past the advance), `test_measure_chars.cpp` |
| FR-9 | `test_text_draw_fonts.cpp` (outline items first in `outline_color`, then the glyph; no outline colour = none; a font without outlines ignores it with one log), `test_outline_bake.cpp` (dilation radius 1..3 by hand) |
| FR-10 | `tests/text/test_text_wrap.cpp` (spec 028's cases kept, + U+2010, U+3000, ID breaks, Hangul at spaces only, every kinsoku table entry at a break, the back-off limit, long words, `max_lines` with `…` and `...`), `tests/text/test_break_class.cpp` (every table range edge; entries ≥ U+2000), `tests/ui/test_ui_seam_equivalence.cpp` |
| FR-11 | `test_text_draw_fonts.cpp` (fallback order, metrics from the glyph's font, line height from the first, replacement glyph, hollow box size and advance, invalid UTF-8 → replacement), `tests/text/test_text_missing.cpp` (logged once per font and code point, 128 then "log full", nothing in pc-release) |
| FR-12 | `tests/text/test_utf8.cpp` extended (every length, U+10FFFF, overlongs, surrogates, truncated sequences, never past the NUL), `test_text_draw_fonts.cpp` (combining marks, variation selectors, ZWJ advance 0 and draw nothing) |
| FR-13 | AC-2 on every compiler and the console; `test_text_draw_fonts.cpp` (integer pens: positions are the float origin + integers); AC-5 |
| FR-14 | `tests/ui/test_ui_fonts.cpp` (slots, proportional label sizes, wrapping, `…`, reveal by characters, choices, list rows, buttons; `set_font` relayouts; stale slot → built-in), `test_ui_seam_equivalence.cpp`, every spec 028 test unchanged, `ui_self_check_pc` `30400DA6` |
| FR-15 | CMake ctests `strings_declaration_*` (LANGUAGES/BASE, missing files, unlisted files, bad codes); `test_strings_check.cpp` (base required) |
| FR-16 | `tools/assetc/tests/test_toml_strings_mode.cpp` (dotted keys, multi-line strings, limits; data mode unchanged), `test_strings_read.cpp` (sections as prefixes, folder form merged, duplicates across files, `[language]` reserved), `tests/loc/test_loc_lookup.cpp` (hash = `UiText::name`'s) |
| FR-17 | `test_placeholders.cpp` (names, `{{`, malformed cases), `test_plural_compile.cpp` (syntax, refusals, required categories), `tests/loc/test_plural_rules.cpp` (the interpreter on every shipped language against CLDR 46's `@integer` samples in `tests/data/plurals/cldr46_integer_samples.txt`, transcribed and cited) |
| FR-18 | `test_strings_read.cpp` (name, fallback, grouping, grouping_min, fonts, plural, limits), `tests/loc/test_loc_number.cpp` |
| FR-19 | `tools/assetc/tests/test_strings_check.cpp` (every error and warning above with its exact text; did-you-mean; `STRICT_STRINGS`), `test_coverage.cpp` (role and fallbacks, `qps` warnings), `test_cli_strings.cpp`; fuzz `libfuzzer_assetc_strings.cpp` |
| FR-20 | `test_strings_writer.cpp` (sorted hashes, buckets, Release without names: no name byte in the file), `tests/loc/test_loc_validate.cpp` (every reason), ctest `strings_incremental` (editing one language's file reconverts only it, its fonts and the index: Ninja's command list) |
| FR-21 | `tests/loc/test_loc_start.cpp` (manifest, block size = the largest chain, tag `strings`, queries, list, `find`, `NotFound`) |
| FR-22 | `tests/loc/test_language_match.cpp` (exact, region swaps, scripts don't match, chains, base; the 3DS table's codes), `test_loc_start.cpp` (player > system > base; the log line) |
| FR-23 | `test_loc_lookup.cpp` (current, chain, base; unknown key: name in Debug/Development once, `??` in pc-release; before start; `resolve`), `test_loc_cost.cpp` (no allocation) |
| FR-24 | `tests/loc/test_loc_format.cpp` (placeholders in any order, `{{`, integers with each grouping style and `grouping_min`, `INT64_MIN`/`MAX`, strings, nested keys, unused arguments, missing arguments Debug vs Release, truncation at a UTF-8 boundary with each code point length, capacity 1 and 0), `test_loc_number.cpp` |
| FR-25 | `tests/loc/test_loc_switch.cpp` (pointers from `tr` valid until the next frame; the switch before dispatch; `LanguageChanged` once with indexes and serial; two calls in a frame: the last wins; an I/O failure restores the previous chain and publishes nothing; the zero guard) |
| FR-26 | the example (stores the code in a spec 027 settings type), `docs/api/localization.md`'s compiled snippet test (`tests/loc/test_docs_snippets.cpp`) |
| FR-27 | `tests/system/test_system_language.cpp` (platform value, override, exclusive Hello → `en`, `off`), conformance (`system_language()` well-formed and stable on fake and PC), `tests/remote/test_remote_wire.cpp` (Language encode/decode, every refusal), `tests/remote/test_remote_session.cpp`, `tools/nnh/tests/test_script_parser.cpp`, `test_script_timeline.cpp`, `language_3ds.cpp` on the console (AC-4) |
| FR-28 | `test_loc_switch.cpp` (following: a system change switches at the next frame; `set_language` turns following off) |
| FR-29 | `tests/loc/test_loc_fonts.cpp` (roles per language, chain roles, shared fonts keep their slot, released fonts, unknown role, fonts never loaded without opting in) |
| FR-30 | `tests/loc/test_loc_ui.cpp` (`apply`: resolver, slots, relayout; a subscription in a scene re-applies on `LanguageChanged`) |
| FR-31 | `tools/assetc/tests/test_pseudo.cpp` (mapping, padding, brackets, placeholders kept, plural forms), ctest (no `qps` in Release outputs) |
| FR-32 | AC-2 (golden on every compiler and the console) |
| FR-33 | `tests/loc/test_loc_debug.cpp` (dump text exactly, show keys, `NNH_LOC_SHOW_KEYS`), `tests/font/test_font_debug.cpp` (dump text, pages view items), pc-release stubs, `check_release_strings.sh` |
| FR-34 | `test_font_cost.cpp`, `test_loc_cost.cpp` (allocation counter unchanged over 100 000 `tr`/`format`/measure/wrap/draw calls), lookup counters (buckets: ≤ 3 compares for every glyph of the CJK fixture) |
| FR-35 | `tests/font/test_font_misuse.cpp`, `tests/loc/test_loc_misuse.cpp` (every case asserts and is harmless on the Release path), `tests/loc/test_loc_random_ops.cpp` (100 000 seeded operations: loads, frees, switches, follows, `tr`/`format`/draws; invariants) under ASan/UBSan |
| FR-36 | include checks (each header pulls only its includes; gfx/UI headers don't include fonts or localization); T8.3 probes and `hello` checks |
| AC-1 | all of the above + fuzz targets: `libfuzzer_assetc_bmfont`, `libfuzzer_assetc_ttf`, `libfuzzer_assetc_png_pages`, `libfuzzer_assetc_font_desc`, `libfuzzer_assetc_strings`; mini-fuzz `fuzz_font_validate`, `fuzz_loc_table`, `fuzz_loc_manifest` in `nnh_tests`; VM 120 with GCC 13/14, clang 18, ASan/UBSan |
| AC-2 | `tests/loc_example/test_self_check.cpp`, ctests `loc_self_check_pc`, `font_fixtures_golden`; the console's `loc: PASS` |
| AC-3 | PC screenshots read back (T7.3), the log lines |
| AC-4 | `check.nnhs` with `--pc` (ctest `loc_check_script_pc`), then on the console; the system-language yes/no |
| AC-5 | console vs `--pc` screenshots: positions identical; empty and full-coverage texels identical; partial-coverage texels' largest channel difference recorded (expected ≤ 2 from blend rounding) |
| AC-6 | the bench lines on the console; T7.7's table |
| AC-7 | T0.1/T8.3 sizes and `nm`, probes, `compare.sh` (text, sprites, ui, platformer pixel-identical), every earlier golden and script, build times (T8.4) |
| AC-8 | T8.2 |
| AC-9 | stage 11 |

### Planned hand mutants (T8.1)

`build/ref-029/mutants.py`, one at a time, `nnh_tests text_ font_ loc_ ui_` and `nnh_assetc_tests` each (≈ 45): kerning
not applied; kerning applied across fonts; centre rounded up; ink extent ignored in the line width; `\r` advancing; NBSP
breaking; U+3000 not a break; ID break between Hangul syllables; kinsoku start check skipped; kinsoku end check skipped;
back-off allowed three times; ellipsis `...` used when the font has `…`; ellipsis on an exact fit; fallback order
reversed; line height from the fallback; replacement before fallbacks; hollow box one pixel wider; missing log not once;
Latin-1 table off by one; bucket shift 4 instead of 5; kerning run search past its count; page codec run length off by
one; validator accepting a record outside its page; equal code points accepted; a path loaded twice not shared; stale
`FontRef` accepted; `tr` returning the base before the current; unknown key marker in Release = name; plural `%` 100
read as 10; plural `!=` as `=`; required categories without the 10¹⁸ powers; grouping at `grouping_min − 1`; `INT64_MIN`
formatted through negation; truncation splitting a code point; placeholder without argument printed in Release;
`{{` stored as `{{`; switch applied immediately; `LanguageChanged` before the block is loaded; serial not bumped; failure
not restoring the previous chain; matching across scripts; region before language-alone; follow not turned off by
`set_language`; coverage check ignoring fallbacks; pseudo padding rounding down; the UI hash folding the built-in slot;
the seam's built-in width from the face instead of cells × cell (must be equivalent: argued or killed). Survivors become
tests or are argued equivalent in "Mutation testing".

## Risks and alternatives

1. **AC-6 targets set before measurement** (`tr` likely missed; Latin and Japanese measuring, `format` and the switch at
   risk). Optimisation list, none changing a glyph item, string or golden: (a) `tr`: an 8-byte hot array `{hash, string
   index}` separate from the cold key data, the bucket index as `u16` pairs in one load, the chain loop unrolled for the
   common one-table hit; (b) measuring: per-face ASCII advance table (128 bytes) read before the record, a kerning bitmap
   per left glyph (skip the run search), the decoder's ASCII fast path inlined (as spec 028's `seam_next`); (c)
   `format`: family-specialised plural code for the four commonest rule shapes chosen at load (a function pointer per
   language: `other`, `n = 1`, `i = 0..1`, Slavic three-way), two digits per division; (d) switch: decode PackBits
   straight into the swizzled LA4 layout (no staging copy), keep the previous language's fonts loaded when the game asks
   (`loc::keep_fonts`, a follow-up) and measure romfs read rates in the bench's breakdown; then AC-6's policy. A language
   switch and `tr` × 1000 per frame are not what games do (the UI resolves once per switch): budgets set from the console
   are acceptable by the policy.
2. **4-bit coverage instead of A8** (question 1). Coverage in 16 levels is visibly slightly coarser at the edges of
   anti-aliased text than 256 levels (the 3DS system font itself is 4-bit). A8 needs `draw2d_3ds.cpp` to recognise A8
   textures (citro2d's multiply tint turns A8's black texels black) and to switch to the solid tint mode per glyph run:
   a new blend path to prove pixel-equal, tint-mode flushes in mixed sprite/text frames (AC-6's 1.15× target), and
   ≈ 50–100 B more in `hello`'s `draw_2d`. The contract already takes 8-bit coverage, so A8 can be added later by
   changing only the 3DS's storage and draw path.
3. **stb_truetype does no range checking** (its README says so). Mitigations: our pre-validator of every offset it
   follows, a zero-padded copy, fuzzing through the converter under ASan; a crash found in stb_truetype that the
   pre-validator can't exclude is refused by a new check (we don't edit the vendored file). The converter runs on the
   developer's machine on fonts they chose; the console never sees a TTF.
4. **Determinism of rasterised bitmaps across hosts**: stb_truetype uses float. Its TU is compiled with
   `-ffp-contract=off` (x86-64 SSE, the 32-bit build's `-msse2 -mfpmath=sse`; arm64 macOS hosts would need the same flag
   — not a supported host today); goldens of converted fixtures catch any difference (AC-2). If a host disagrees, the
   fallback is to commit the converted test fonts (the bytes the console reads are the converter's output anyway).
5. **romfs throughput** (switch target) has never been measured on the Old 3DS: the bench logs reads separately; the
   page codec halves the bytes (two pixels per byte) and PackBits removes empty space (≈ 3–5× on glyph pages).
6. **The UI seam refactor** touches ≈ 140 cell uses in 17 files; risk of a silent layout change. Mitigations: the
   equivalence test against a frozen copy of the old wrapper, spec 028's unchanged expectations, the golden and the
   console's `ui: PASS 30400DA6` (the seam section above).
7. **Stale `tr` pointers after a switch** (a UI or cached string not re-applied): text from the new block shows (garbled
   or another string) but never memory outside the block (zero guard). Documented; Debug/Development could poison the
   block's old range for one frame (a follow-up if games hit it).
8. **The OFL font** (question 3): an OFL subset is a Modified Version; the subset is renamed (no Reserved Font Name
   used), keeps `OFL.txt` beside it and in the package; the maintainer approves the exact upstream file. A font change
   later only changes the example's goldens.
9. **CLDR plural data licence**: CLDR is under the Unicode License v3 (permissive, notice when redistributing data
   files); we transcribe the integer rules of 36 languages and the sample integers as test data, credited in
   `docs/REFERENCES.md` with the notice; no CLDR file is copied whole.
10. **Two vendored libraries in the host tool** grow `nnh-assetc`'s clean build (≈ 3–4 s); only games with assets that
    need `nnh-assetc` build it.
11. **Merging**: single worktree from `main`; insertions at distinct anchors (`engine/core/CMakeLists.txt` after the UI
    block, `tests/CMakeLists.txt` after the UI block, `examples/CMakeLists.txt` after `ui`, `nnh_assets.cmake` before the
    data rule), rebased before stage 8 and the merge.

## Decisions taken (to review)

The spec's open questions are decided as it recommends. Plan-level decisions:

1. **Names**: modules `font` (`<nnh/font.hpp>`, `src/font/`) and `loc` (`<nnh/localization.hpp>`, `nnh::loc`,
   `src/loc/`), shared text code in `src/text/`; tags `font` (shared with the built-in texture) and `strings`; log
   channels `font`, `loc`, `text`; example `examples/localization` (`LOC_DEMO`, `LOC_SHOT`); assets `.nnhfont`,
   `.nnhstr`, `.nnh_loc`.
2. **Glyph pages: 4-bit coverage**, contract `texture_create_alpha` (question 1).
3. **BMFont**: text and XML forms; pages named `<font>_<n>.png` beside the `.fnt`, pipeline-owned, decoded by
   `stb_image` (question 2) and re-packed under D49's rules.
4. **One description per size** (question 4).
5. **Fonts are reached through a table of 32 refcounted entries** and `FontRef`s; gfx and the UI through
   `g_font_resolve`, set by the first `load_font`.
6. **Glyph lookup**: Latin-1 table + BMP buckets (`cp >> 5`) derived at load; kerning runs per left glyph.
7. **Line breaking**: spec 028's rules + ID breaks + JIS X 4051 kinsoku at ID breaks only, one back-off rule; every new
   table entry ≥ U+2000.
8. **Languages declared in CMake** (`LANGUAGES`, `BASE`, `STRICT_STRINGS`); a language is one file or one folder.
9. **TOML strings mode** (dotted keys, multi-line basic strings, 1 MiB) inside spec 014's reader; `[language]` reserved.
10. **Plural rules** as CLDR 46 text in the converter, bytecode in the manifest, interpreter in core; a game rule in
    `[language.plural]`.
11. **The switch** at the next frame through a D32 after-input hook; `start()` immediate; `LanguageChanged` also after
    `start()`.
12. **`tr` before `start()`** returns `""` (assert); unknown keys: name (Debug/Development) / `??` (Release).
13. **Role fonts only when opted in** (`use_fonts`, `font`, `apply`), so strings-only games link no font code.
14. **Simulator language** from `NNH_SIM_LANGUAGE` and `language.ini` (no hotkey: `window_pc.cpp` and `hello` stay as
    they are; a key is a follow-up); scripts own the language from their Hello (`en`).
15. **Remote**: message type 8 `Language`, the 8 code bytes packed into the decoded message's `press`/`release` (the
    schedule's 32-byte message stays 32 bytes); `language CODE|off`.
16. **Page codec**: two pixels per byte + PackBits; colour pages RGBA8 + PackBits.
17. **Example**: one OFL subset serves Latin, Cyrillic and Japanese (question 3), an original HUD pixel BMFont, the base
    in the folder form with a generated 64 KiB bench section; `examples/ui` keeps its hand-written resolver and golden.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Two contract functions in their own files per platform with conformance tests; every rule integer and shared; bytes baked by the build; goldens on every compiler and the console |
| II | ✅ | 20 B per glyph, 1 byte per page pixel, CJK subset from strings, ≤ 8 pages ≤ 1024, one block per chain, the example ≈ 1.45 MiB peak; glyph cost ≈ built-in + 3 %; four AC-6 targets at risk with an optimisation list and the bench policy |
| III | ✅ | Light headers measured; per-file converters with depfiles off the game-code path; host-only third-party code; times measured (T8.4) |
| IV | ✅ | Queries; one `LanguageChanged` per switch; no module dependency from gfx/UI to localization |
| V | ✅ | No allocation, `printf`, `virtual`, RTTI or exceptions in `tr`, `format`, measuring, wrapping, drawing |
| VI | ✅ | Tests first; CLDR samples; equivalence with the old wrapper; goldens; fuzzing; random operations; mutants |
| VII | ✅ | ≈ 30 core sources and ≈ 26 converter sources, each one concept ≤ 250 lines; one header per module |
| VIII | ✅ | Two API pages, a guide, linked pages, the example, D49, D50 |
| IX | ✅ | Vendored public-domain/MIT code unmodified and pinned; original test and HUD fonts; one approved OFL subset with its licence; CLDR credited |
| X | ✅ | Every parser bounded and fuzzed; stb_truetype behind a pre-validator; run-time validators return `Result` |
| XI | ✅ | Separate and optional; `hello` (Release) byte-identical; probes prove what links |

No constitution exception is requested. D49 and D50 are new (status: design, for review); D28 gains the `Language`
message (additive in version 1, as `Slider`); D32 gains a user; D48's seam is amended inside the module (its API and
golden unchanged). Flagged for the maintainer: questions 1–5 below and the AC-6 targets at risk (risk 1).

## Questions for the maintainer (answered 2026-11-26: each as recommended)

Answers: (1) 4-bit coverage pages (LA4 on the 3DS) instead of A8 — this amends the spec's FR/open-question choice;
A8 can come later on the 3DS side only. (2) Vendor `stb_image` (PNG only, host-only). (3) A Noto Sans JP OFL subset
renamed `NNHGE Example Sans JP`; the upstream file's version and SHA-256 still go to the maintainer before it is
committed (T7.1). (4) One size per font description. (5) Accept the Debug/Development `hello` growth from the remote
`language` message; Release `hello` stays byte-identical.

1. **Glyph pages: 4-bit coverage (recommended) or 8-bit A8?** Recommended: coverage rounded to 16 levels at build time,
   stored as LA4 on the 3DS (1 byte per pixel, the same memory as A8) and drawn by today's multiply path — `hello`
   byte-identical, no new blend path to prove pixel-equal, no tint-mode switches between text and sprites, half the
   bytes to read on a switch (the 3DS system font is 4-bit too). A8 (the spec's first choice) gives smoother edges but
   needs the 3DS `draw_2d` change described in risk 2. The contract takes 8-bit coverage either way.
2. **Vendoring `stb_image` (PNG only, host-only) for BMFont pages?** Recommended: yes — same repository, licence and
   pinning as `stb_truetype`, fuzzed through the converter. Alternatives: BMFont pages packed as ordinary RGBA textures
   (4× memory, no re-packing, no new parser), or our own PNG decoder (inflate + filters, ≈ 500 lines to own).
3. **The example's Japanese font**: recommended a subset of **Noto Sans JP** (OFL 1.1; CFF outlines, so the CFF path is
   exercised; Latin, Cyrillic, kana and the kyōiku kanji in ≈ 250–300 KB), renamed `NNHGE Example Sans JP`, subset by a
   committed `fontTools` command from an upstream file whose version and SHA-256 you approve. Alternative: M PLUS 1p
   (OFL, TrueType outlines, a larger subset).
4. **One size per font description** (recommended: the build's asset list is made at configure time from file names, so
   one file → one asset keeps the pipeline simple; a second size is a second short file) or a `sizes = [12, 16]` list
   (needs CMake to read the description at configure time, and a reconfigure when the list changes)?
5. **Development-flavour `hello` growth**: the remote `language` message adds ≈ 150–250 B to the Debug/Development `hello`
   (decoder case + override), as spec 026b's `Slider` did; Release `hello` stays byte-identical. Accept (recommended), or
   ask for a Development budget of 0 (then the language override needs a second remote path linked only by localized
   games)?

## Implementation notes

### Stage 0 results (2026-11-26, worktree `spec-029` at `8d9c5c5`, Windows, WinLibs GCC 16.2)

Scripts in `build/ref-029/` (not committed): `hello_snapshot.sh` (sizes, `nm -C`, SHA-256), `compare.sh` (reference
screenshots), `times.sh` (edit/clean/header times), `micro_ui.cpp` + `build_micro.sh` (the UI micro-benchmark).

- **`hello`** (`size`: text / data / bss; file bytes): 3ds-release 202 204 / 28 360 / 107 368, `.3dsx` 247 628 B,
  `hello.elf` SHA-256 `506034e0…b8b96035`, `.3dsx` `ade4dcef…5600ff8c9`; 3ds-dev 220 488 / 32 872 / 109 976, `.3dsx`
  270 652 B; pc-release 134 880 / 22 052 / 212 400, 285 885 B; pc-debug 206 276 / 26 820 / 215 152, 1 339 674 B. `nm -C`
  lists kept (3ds-release 1 377 lines, 3ds-dev 1 479, pc-release 3 001, pc-debug 4 953); 3ds-release `texture_3ds.o`
  SHA-256 `0b318282…90b80339`.
- **3ds-release `.text`**: `text` 189 956 B, `sprites` 207 132 B, `ui` 301 116 B, `platformer` 396 340 B.
- **Build times**: edit `examples/ui/title_scene.cpp` → pc-debug 2.75 / 2.10 / 2.09 s, 3ds-release `.3dsx` 5.21 / 4.02 /
  6.23 s; clean `nnh_core` (pc-debug) 21.9 s; clean `nnh-assetc` (pc-debug) 7.7 s.
- **Header cost**: a one-line TU with `<nnh/gfx.hpp>` + `<nnh/result.hpp>`: 7 844 preprocessed lines, `-fsyntax-only`
  0.45 s.
- **UI micro-benchmark**: plan 028's `build/ref-028/micro` isn't in the tree, so `build/ref-029/micro_ui` was written
  again from plan 028's description (examples/ui's bench workloads in tight loops, pc-release `-O2`, the fake platform)
  and its baseline recorded before any change: relayout128 best of runs 10.85–11.56 µs (one cold run 14.8), **draw hash
  `367085C2`** (40 frames of the 128-widget UI and both lists; it stands in for plan 028's `7429F9EB`, whose program is
  gone). The T0.1 executable is kept (`micro_ui_t01.exe`) to rerun side by side.
- **Reference screenshots** (T0.2): 15 PNGs in `build/ref-029/ref/`; `compare.sh` retakes them identical except
  `sprites_bench` and `platformer_bench`, which differ run to run already at `8d9c5c5` (timing-driven) and are only
  compared by eye.
- **Tests** (T0.3): `nnh_tests` 3 582 tests, 0 failures; 514 left under `kMaxTests` (4096) → raised to 8192.

### Stage 1 results (2026-11-26)

- ctest pc-debug 346/346 and pc-release 318/318 pass; 3ds-release and 3ds-dev build without a warning.
- **Release `hello`**: 3ds-release `hello.elf` and `.3dsx` byte-identical to T0.1 (SHA-256 above), `size` and `nm -C`
  identical; pc-release `size`, `nm` and bytes identical.
- **Debug/Development `hello`** (the remote `language` message, question 5): 3ds-dev text +432 B (220 488 → 220 920),
  bss +16 B, `.3dsx` +440 B (270 652 → 271 092); pc-debug (unoptimised) text ≈ +3.4 KB. Over the plan's ≈ 150–250 B:
  the first version (decoder and override checking the full tag rules) added 952 B; the full rules then moved out of
  the console's path (see the refinements below), leaving the decoder's shape check, the 16-byte override and the
  session's apply/off/exclusive/detach calls. Reported to the maintainer.

### Stage 2 results (2026-11-26)

- Every stage-end guard holds: ctest pc-debug 346/346, pc-release 318/318; 3ds-release and 3ds-dev without a warning;
  3ds-release `hello.elf`/`.3dsx` byte-identical to T0.1, pc-release `hello` size/`nm`/bytes identical; `compare.sh`:
  every reference screenshot identical (the UI's included); `ui_self_check_pc` `30400DA6`.
- `tests/ui/test_ui_seam_equivalence.cpp`: the shared wrapper through the built-in face equals the frozen pre-029 wrapper
  on 100 000 seeded Latin-1 strings × widths 1..60 cells × max lines 0..4 × scales 1..3 (lines, bytes, widths, ellipsis
  flags), and the longest word on 20 000 strings. Spec 028's `test_wrap.cpp` cases moved to `tests/text/test_text_wrap.cpp`
  with their expected lines unchanged (widths cells × 8); `tests/ui/test_text_seam.cpp` calls the moved wrapper (the
  only edit in `tests/ui/`).
- UI micro-benchmark: draw hash `367085C2` unchanged; relayout128 side by side with T0.1's executable: 10.56–11.33 µs vs
  10.59–11.39 µs (within noise).
- 3ds-release `.text`: `ui` 301 116 → 304 916 B (+3.8 KB: the shared wrapper's ID breaks and kinsoku, the break table);
  `text`, `sprites`, `platformer` unchanged.

### Stage 3 results (2026-11-26)

- ctest pc-debug 346/346, pc-release 318/318; 3ds-release and 3ds-dev without a warning; `compare.sh` identical (the
  UI's screenshots included); `ui_self_check_pc` `30400DA6`; micro-benchmark draw hash `367085C2`, relayout128 ≈ 10.4 µs.
- **`hello`**: 3ds-release `.3dsx` and the stripped `.elf` byte-identical to T0.1; the unstripped `.elf` differs only
  in the order of DWARF strings (`gfx.hpp` gained declarations; every edit above `draw` kept its line numbers, the new
  ones are at the end). pc-release `hello` size, `nm` and bytes identical.
- Headers: `<nnh/gfx.hpp>` + `<nnh/result.hpp>` 7 887 preprocessed lines (T0.1: 7 844), `<nnh/font.hpp>` 7 959; parse
  time within noise of T0.1's 0.45 s.
- 3ds-release `.text`: `text` 189 956 → 197 860, `sprites` 207 132 → 214 948, `platformer` 396 340 → 404 412 (+≈ 8 KB
  each: `draw_text` dispatches to the layout path whenever an option asks for it, so a game that draws text links that
  path even without a font); `ui` 304 916 → 310 276. To be weighed by the maintainer (a split dispatch would keep the
  built-in-only games at their old size).

### Stage 4 results (2026-11-26)

- `stb_truetype.h` v1.28 and `stb_image.h` v2.32 from `nothings/stb` commit `9f755b604db3492df2cbeb39bf84b81e3e3932e3`,
  unmodified (SHA-256 in each `README.md`), host-only object libraries linked into `nnh-assetc`.
- Test fonts: `scripts/make_test_fonts.py` (fontTools' FontBuilder, the maintainer's pre-approved Python tool, instead of
  a stdlib-only writer) writes `tests/data/fonts/`; reruns give the same bytes.
- `nnh_assetc_tests`: `test_charsets`, `test_font_desc`, `test_cli_font` (both sources end to end, core's validator
  accepts every output). Not yet written: the per-parser suites of T4.3/T4.6/T4.7/T4.8 (`test_bmfont_text`, `test_ttf_check`,
  `test_ttf_raster`, `test_glyph_pack`), the `font_routing`/`font_fixtures_golden` ctests and the T4.10 fuzzers.

### Stage 5 results (2026-11-26)

- `nnh_assetc_tests` 298/298 (`test_strings`: placeholders, the 36 shipped rules against CLDR 46 samples, checks with
  their exact messages, tables accepted by core's validator in Debug and Release forms, the index and `qps`).
- Not yet written: T5.1's TOML strings mode (string files have their own reader, see the refinements), T5.7's
  `strings_declaration_*`/`strings_incremental` ctests, T5.8's assetc fuzzer.

### Stage 6 results (2026-11-26)

- `nnh_tests` 3 776 (pc-debug) / 3 714 (pc-release) pass; 55 `loc_` tests and 4 `plural_` tests: validators (each
  reason), start (player > system > base, the pseudo-language only by choice, `LanguageChanged` once, damaged files leave
  nothing), lookup (chains, markers, show keys, 300 keys), format (grouping per language, en/ru/ja plurals, texts, keys,
  `{{`, a missing argument, UTF-8 truncation), switching (next frame, last call wins, failure keeps the language, following
  the system), role fonts and `apply` (chain roles, no leak over switches, unknown role), 100 000 random operations, no
  allocation after start, and mini-fuzz of tables and manifests (20 000 mutations each, re-sealed).
- 3ds-release/3ds-dev build without a warning; `hello` (3ds-release, 3ds-dev stripped/`.3dsx`, pc-release) identical to
  stage 3's; `.text` of `text`, `sprites`, `ui`, `platformer` unchanged (localization is linked only when called).

### Stage 7 results (2026-11-26)

- **T7.1, the Japanese font** (pre-approved by the maintainer 2026-11-26): upstream
  `https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf`,
  noto-cjk release `Sans2.004`, SHA-256 `dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073` (4 533 028 B);
  licence `Sans/LICENSE` at the same commit, OFL 1.1, SHA-256 `6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2`.
  Subset `examples/localization/assets/fonts/src/NNHExampleSansJP.otf` ("NNHGE Example Sans JP"), **268 956 B**, 1 671 code
  points, SHA-256 `9159b504457730539cd4b63dbe0abf5203e02a328eeffa8b6c3c09d408ead590`, made by
  `examples/localization/fonts/make_subset.py` (fontTools 4.63; byte-identical on reruns; layout features `kern`/`palt`
  only: with every feature it was 401 KB). Licence packed as `assets/licences/OFL-NNHExampleSansJP.txt` with the font's
  copyright notice (`© 2014-2021 Adobe`) prepended. Provenance also in `examples/localization/fonts/README.md`.
- Converted fonts (pc-debug): `body_16` 280 glyphs 82 KB, `body_ja_13` 1 358 glyphs one 1024 × 512 page 295 KB,
  `title_32` 238 glyphs and `title_ja_32` 161 glyphs one 512 × 512 page each (outline 2), `bench_1024` one 1024 × 1024
  page, `hud` (BMFont, 22 glyphs). Tables: en 78 KB in Release (95 KB Debug, with names), the others 1.4–1.8 KB.
- Build output: notes for characters the subset lacks (U+FFFD, part of Cyrillic) and one warning per language for the
  1 024 base-only bench keys (+ pt-BR's deliberate `menu.continue`); so the example is **not** `STRICT_STRINGS`-clean
  (deviation from T7.1's test: base-only keys can't be marked).
- Self-check: checkpoints en `92FEBB1D`, pt-BR `A166BC22`, de `1DE842A2`, ru `7E4FE058`, ja `21809981` =
  `kLocCheckGolden`; pc-debug and pc-release agree (Release read from its screenshot: Release has no log).
  `tests/check.nnhs` passes twice with `--pc` (116 lines, frames 1–3 699). 3ds-release and 3ds-dev build the example
  without a warning (`.3dsx` 1 137 852 / 1 323 388 B).
- Bench on the PC (pc-dev, PC numbers): base submit 0.017 ms; **glyphs builtin 0.57 µs, font 0.82 µs per glyph, ratio
  1.43 (target ≤ 1.15: FAIL on the PC)**; frame worst 17.5 ms, 0 missed; measure_latin 0.056 ms; measure_ja 0.062 ms;
  tr1000 0.042 ms; format100 0.018 ms; page1024 load + release ≈ 15 ms; memory body 82 KB, ja 295 KB. The switch phase
  reads the hook's profile scope (`loc switch`); its read was fixed after the run above (it read a frame too early).
- Deviations: one showcase scene per screen sharing a `LocScene` base (no separate `roles.hpp`/`settings.*`: the
  player's choice is not saved in the example — `set_language` only); the self-check folds `measure_text` and
  `measure_chars` (pure, the same items' positions) instead of the internal layout items, and the `nnh_tests` copy of
  the self-check (T7.2's `tests/loc_example/`) is not written: the example's ctest `loc_self_check_pc` covers it.

### Stage 7 ARM pre-check (3ds-dev objects, 2026-11-26)

- `__aeabi_*` calls: none in `src/loc/*` after the fix below, none in `src/text/*` or `src/font/*`. The plural
  interpreter called `__aeabi_uldivmod` (64-bit `n % 10^k`); it now reduces `n` to `n mod 10^6` byte by byte with
  32-bit constant divisions (tests: `plural_moduli_of_numbers_beyond_32_bits`, the CLDR samples).
- VFP instructions: `text_draw.o` 32, `layout.o` 3, `text_measure.o` 2, `font_debug.o` 8 (Debug only) — the final
  positions and colours handed to gfx; not yet compared one by one with the plan's table, nor `-fstack-usage` measured
  (left for T10.1 with the console numbers).
- Expected console numbers: the plan's table stands (glyph ratio and `tr1000` at risk; the PC's 1.43 glyph ratio is a
  warning sign for T10.2).

### Stage 8 results (2026-11-26)

- Mutation: 19 hand mutants on the localization runtime (`build/ref-029/mutants.py`: number grouping, plural operands
  and negation, form fallback, chain walk, UTF-8 truncation, placeholder spans, the language of a key's table, switch
  no-op and failure restore, `set_language` ending following, region-less matching, pseudo-language by system,
  validator checks, role fonts from the chain): 16 killed at first; the 3 survivors became tests
  (`loc_format_uses_the_rule_of_the_language_whose_text_it_is`, `loc_format_a_form_the_key_lacks_is_its_other_form`,
  `loc_start_prefers_the_language_without_a_region`) and are killed. Font/text mutants, T8.2 (release strings script),
  T8.3 (link probes) and T8.4 (build times) were not run.
- `hello`: 3ds-release/3ds-dev stripped `.elf` and `.3dsx`, pc-release `size`/`nm`/bytes identical to stage 3's after
  stages 6 and 7.

### Stage 10: console run (prepared)

On the Old 3DS: Homebrew Launcher → **Y**, nobody touching the console, then on the PC:

1. `build/pc-debug/tools/nnh/nnh run localization --script examples/localization/tests/check.nnhs` (3ds-dev build).
   Expected, in order (recorded from the `--pc` run): `loc: showcase`; `language pt-BR (system)`; `language de (system)`;
   `loc: focus shop`; `loc: shop 1 1 Laterne`; `loc: shop 21 ru 323120D184D0BED0BDD0B0D180D18C`;
   `loc: shop 21 ja E383A9E383B3E382BFE383B33231E5808B`; `loc: focus shop`; `loc: focus dialog`; `loc: dialog shown …`;
   `loc: focus dialog`; `loc: check started`; `loc: en sum 92FEBB1D`; `loc: pt-BR sum A166BC22`; `loc: de sum 1DE842A2`;
   `loc: ru sum 7E4FE058`; `loc: ja sum 21809981`; `loc: PASS 21809981`; `loc: focus dialog`; `loc: focus options`;
   `language …` (Options → Right: the pseudo-language `qps` in Development); `loc: focus options`; `loc: focus fonts`;
   `loc: fonts hud …`; `loc bench: started`; `loc bench: base gfx submit …`; `loc bench: glyphs builtin … ratio …
   PASS|FAIL`; `loc bench: frame worst … missed … PASS|FAIL`; `loc bench: measure_latin …`; `loc bench: measure_ja …`;
   `loc bench: tr1000 …`; `loc bench: format100 …`; `loc bench: switch avg … worst …`; `loc bench: page1024 …`;
   `loc bench: memory body 280 glyphs 82168 B ja 1358 glyphs 295000 B`; `loc bench: PASS`. Screenshots in
   `build/logs/localization-<date>/` (title, shop, ru, dialog, options, gallery), to compare with the `--pc` run's.
   The script's last line fails if a bench target is missed (`loc bench: FAIL (…)`): then T10.3.
2. `build/pc-debug/tools/nnh/nnh run ui --script examples/ui/tests/check.nnhs` → `ui: PASS 30400DA6`, relayout < 0.93 ms,
   draw ratio ≤ 1.15.
3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` → `bench: PASS …`.
4. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the replay lines, `60 Hz PASS`.
5. Yes/no: System Settings → language Português (or Deutsch), HBL → Y, `build/pc-debug/tools/nnh/nnh run localization`
   (no script): the log shows `language pt-BR (system)` (or `de`; the 3DS reports Portuguese as `pt-BR` or `pt-PT` by
   region: either starts in pt-BR) and the title is in that language. Then the setting goes back.

### Implementation refinements

- **Stage 1.** (1) `texture_3ds.cpp` is unchanged: `Texture3ds` keeps its anonymous-namespace definition there and
  `texture_3ds.hpp` holds a copy of the same layout (static_assert) for `texture_alpha_3ds.cpp`, because moving the
  struct changed the debug information of every game's `.elf` (hello's included; its `.3dsx` stayed identical).
  (2) New contract declarations go at the end of `video.hpp`/`system.hpp`, after the existing ones, so their lines (in
  every game's DWARF) don't move — inserted in the middle, they changed `hello.elf`'s bytes. (3) The tag rules
  (`system/language_tag.hpp`) are a header-only `constexpr` check for the PC platform library (which includes the header
  through a private include path and links nothing of core) and the `nnh` tool, plus one out-of-line copy for core
  (`language_tag_valid`). (4) The remote decoder checks a Language code's *shape* (2 lowercase letters, then letters,
  digits, `-`; NUL padding, no interior NUL); `pack_language` (the tool) and `system::language()` check the full rules,
  and `set_language_override` copies what it is given — an ill-formed override is ignored by `system::language()`
  (the platform's language shows). This keeps the tag rules out of every Debug/Development game. (5) The PC's
  `language.ini` is read only when the simulator remembers its view (never in scripted runs: `NNH_EXIT_AFTER_FRAMES`,
  `nnh run`), so screenshots stay `en` unless `NNH_SIM_LANGUAGE` says otherwise. (6) The fake's `set_system_language`
  takes `nullptr` (the platform answers null) to prove core's "never null" guard.
- **Stage 2.** (1) The `.nnhfont` header has one more flag, `kFlagBmp` (a glyph in U+0100..U+FFFF), set by `font_write`
  and checked by the validator, so the loader sizes the block (BMP buckets or not) from the header before it reads the
  code points. (2) The writer takes pages already in their decoded form (A4 packed by `a4_pack`, or RGBA8), so it needs
  no buffer of its own (core never allocates in it). (3) Buckets: a full bucket (32 glyphs) is read directly; otherwise
  a binary search (≤ 3 compares on a ≈ 3 000-kanji table: tested). (4) Kinsoku in the wrapper: only boundaries between
  two ID characters are break opportunities; a forbidden one moves back one or two characters (never to the line's
  start), else stays — so "あいぁぁぁう" at three characters wraps "あ|いぁぁ|ぁう". (5) `WrapLine` carries `width` (pixels
  at scale 1) and `chars` (what spec 028 called cells: code points but '
'); the wrapper takes a width in pixels at the
  face's scale; a wrapping UI label with the built-in font passes `cells × cell`, so it wraps exactly as before.
- **Stage 3.** (1) `load_font` reads the whole `.nnhfont` once (`assets::load`), validates it, copies the tables into the
  block and decodes each page into a temporary buffer — instead of streaming sections — because the files are small
  next to their pages and one validated buffer keeps the loader simple. (2) Error codes: a damaged font is `Corrupted`,
  the 33rd font `LimitReached` (the engine's existing codes; the plan said `InvalidData`/`Unsupported`). (3) `gfx.hpp`
  edits keep every existing declaration on its line (one merged line, `FontRef` after `kFontLineHeight`), new
  declarations at the end — the hello criterion became "`.3dsx` and stripped `.elf` identical" (the DWARF string order of
  the unstripped `.elf` moves). (4) Release skips the shutdown hooks, so the font table checks that an entry's first
  page is still a live texture (`alive()`) and forgets entries of an earlier engine run.
- **Stage 4.** (1) The test fonts come from fontTools (see Stage 4 results). (2) BMFont pages and TrueType sources are
  not packed; the `.font`/`.fnt` step owns them.
- **Stage 5.** (1) String files have their own small reader (`strings_read.cpp`: sections, dotted keys, basic and
  multi-line strings, the `[language]` tables) rather than a mode of the data TOML reader — it keeps the data pipeline
  untouched; it accepts the plan's syntax. (2) Header sizes: `.nnhstr` 80 bytes, `.nnh_loc` 48 bytes (one more offset
  each than sketched). (3) Coverage is checked with each language's first role font (its own or its chain's) and that
  font's fallbacks.
- **Stage 6.** (1) The after-input hook is registered by `start()` and stays registered until `stop()` (one branch per
  frame when nothing is pending), so `set_language()` needs no registration. (2) Matching lives in `loc_state.cpp`
  (`match_language`) — no `loc_match.*` files; `tests/loc/test_loc_start.cpp` covers it (exact, same language other
  region, pseudo-language never by system). (3) Release skips shutdown hooks: a Release game that restarts the engine
  calls `loc::stop()` first (documented on `stop()`); the module's role fonts live in a union so no static destructor
  runs after the engine is gone. (4) Core tests build tables and manifests with a small writer in
  `tests/loc/loc_fixture.hpp` (the layout of `strings_check.cpp`/`strings_index.cpp`), so each test states exactly its
  languages, chains, rules and fonts and can corrupt them; `nnh_assetc_tests` checks that the real writer's output passes
  the same validators. (5) `nnh_tests` links libstdc++/libgcc statically on MinGW: Git Bash puts its own msvcrt-based
  `libstdc++-6.dll` first on PATH, and a string that DLL allocated, freed by an inlined Release destructor through the
  UCRT, corrupted the heap (found by the localization fixtures; tests only, games use no C++ runtime allocation).
  (6) `tr()`'s show-keys path and `format()`'s "{name}" marker are compiled only in Debug/Development (`#if`), so Release
  keeps no key names.
