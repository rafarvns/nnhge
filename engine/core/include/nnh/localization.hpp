#pragma once
/// @file nnh/localization.hpp
/// Localization (spec 029, decision D50): text by key in the game's languages, converted from string files by the build
/// (`nnh_add_game(... LANGUAGES en pt-BR ja BASE en)`, strings/<code>.strings); one current language, module-level, chosen
/// at start (the player's saved choice, else the system language, else the base) and switched at the start of the next
/// frame; placeholders and CLDR plural forms without printf or allocation; font roles per language; spec 028's widgets
/// through the resolver. Optional: a game that never calls loc:: links none of it. See docs/api/localization.md.
///
/// @code
/// if (!nnh::loc::start({.language = settings.language})) return false;
/// nnh::gfx::draw_text(nnh::Screen::Top, {8, 8}, nnh::loc::tr("menu.new_game"));
/// char line[64];
/// nnh::loc::format(line, sizeof line, "items.count", nnh::loc::arg("count", coins));
/// @endcode

#include <concepts>
#include <cstddef>
#include <cstdint>

#include "nnh/detail/name_hash.hpp"
#include "nnh/gfx.hpp"
#include "nnh/result.hpp"

namespace nnh {

class Ui;
struct MemTag;

/// Published when a language switch took effect (FR-25): at the start of the frame, before that frame's dispatch, and
/// once after start() (previous = loc::kNoLanguage). Indexes into the shipped list; serial = loc::serial().
struct LanguageChanged {
    std::uint8_t previous = 0xFF;
    std::uint8_t current = 0xFF;
    std::uint16_t reserved = 0;
    std::uint32_t serial = 0;
};

namespace loc {

inline constexpr std::uint8_t kNoLanguage = 0xFF;
inline constexpr int kMaxLanguages = 32;
inline constexpr int kMaxRoles = 8;

/// A key: the FNV-1a 32 hash of its dotted name ("menu.new_game"), computed at compile time for a literal — the same
/// hash UiText::name uses, so widgets and tr() meet without a generated header.
struct Key {
    std::uint32_t hash = 0;
#if !defined(NNH_FLAVOR_RELEASE)
    const char* name = nullptr; ///< for the Debug/Development marker and logs; absent in Release
#endif
    constexpr Key() = default;
    template <std::size_t N>
    consteval Key(const char (&text)[N]) // NOLINT: implicit on purpose (tr("menu.new_game"))
        : hash(detail::fnv1a32(text, N - 1))
#if !defined(NNH_FLAVOR_RELEASE)
          , name(text)
#endif
    {
    }
    constexpr explicit Key(std::uint32_t h) : hash(h) {}
};

/// A key from a name known only at run time (data-driven text), hashed now. The name must outlive its uses.
Key key(const char* name);

struct StartOptions {
    const char* language = nullptr; ///< the player's saved choice (a shipped code), or null
    bool follow_system = false;     ///< switch when system::language() changes (FR-28)
    MemTag* tag = nullptr;          ///< default: the `strings` tag (Heap)
};

/// Reads the manifest `.nnh_loc`, allocates one block of the largest language chain's bytes and loads the starting
/// language: `options.language` if shipped (logged "player"), else system::language() matched (exact, then the same
/// language with another region; "system"), else the base ("base"). Publishes LanguageChanged. Load time (allocates).
/// Errors: NotFound (no `.nnh_loc`: the game declared no LANGUAGES), Corrupted (a damaged manifest or table),
/// OutOfMemory. Calling it twice asserts and changes nothing.
Result<void> start(const StartOptions& options = {});
/// Frees everything and unregisters the frame hook; tr() then returns "". nnh::shutdown() does it in Debug/Development;
/// Release skips shutdown hooks, so a Release game that restarts the engine calls stop() before nnh::shutdown().
void stop();

/// Switches at the start of the next frame (FR-25): until then every pointer tr() returned stays valid. A later call
/// in the same frame wins. NotFound (and no change) for a code not shipped. Turns follow_system off.
Result<void> set_language(const char* code);
/// Follows system::language() (checked every frame; a change switches at the next frame).
void follow_system(bool on);

const char* code();         ///< the current language's code ("" before start)
const char* name();         ///< its display name, in itself ("日本語")
int index();                ///< its index in the shipped list, or −1
std::uint32_t serial();     ///< bumped by every switch that took effect (1 after start)
int language_count();
const char* language_code(int i); ///< "" outside 0..count−1
const char* language_name(int i);
int find(const char* code);       ///< the index of a shipped code (case-insensitive), or −1

/// The current language's text of `key`, else its fallback chain's, else the base's. A key no table has: its name in
/// Debug/Development (logged once), "??" in Release. Before start(): "" (asserts). Never null; valid until the next
/// switch takes effect. No allocation; a bucket and one or two compares per table.
const char* tr(Key key);
/// tr() by hash, for Ui::set_resolver(&nnh::loc::resolve).
const char* resolve(std::uint32_t key);

/// A named argument of format().
struct Arg {
    std::uint32_t name = 0;
    std::uint8_t kind = 0; ///< 1 integer, 2 text, 3 another key's text
    long long number = 0;
    const char* text = nullptr;
    std::uint32_t key = 0;
};
template <std::integral T>
constexpr Arg arg(Key name, T value) {
    return Arg{name.hash, 1, static_cast<long long>(value), nullptr, 0};
}
inline Arg arg(Key name, const char* utf8) { return Arg{name.hash, 2, 0, utf8, 0}; }
inline Arg arg_key(Key name, Key key) { return Arg{name.hash, 3, 0, nullptr, key.hash}; }

/// Writes `key`'s text with its placeholders replaced into `out` (integers grouped as the language says, texts, other
/// keys' texts) and the plural form its plural argument chooses (CLDR integer rules). Truncated at a UTF-8 boundary to
/// fit `capacity − 1` bytes, NUL-terminated; returns the bytes written. A placeholder without an argument: "{name}" in
/// Debug/Development (logged), nothing in Release. No allocation, no printf.
int format(char* out, int capacity, Key key, const Arg* args, int count);
template <typename... A>
int format(char* out, int capacity, Key key, const A&... args) {
    const Arg list[] = {Arg{}, args...};
    return format(out, capacity, key, list + 1, static_cast<int>(sizeof...(A)));
}

// Fonts per language (FR-29): linked only when one of these is called.
/// Loads the current language's role fonts now and at every switch ([language.fonts] in the string files).
void use_fonts();
/// The current language's font for `role` ("body", "title"…; a role a language lacks comes from its chain); FontRef{}
/// (the built-in font) for an unknown role (logged once in Debug/Development).
FontRef font(Key role);

/// A UI font slot and the role it shows (apply()).
struct UiFontRole {
    int slot = 0;
    Key role{};
};
/// Spec 028's widgets in the current language (FR-30): the UI's resolver = resolve, each slot = font(role), then
/// Ui::text_changed(). Call it in on_enter() and from a LanguageChanged subscription the scene owns.
void apply(Ui& ui, const UiFontRole* roles, int count);
template <std::size_t N>
void apply(Ui& ui, const UiFontRole (&roles)[N]) {
    apply(ui, roles, static_cast<int>(N));
}

// Debugging (FR-33), Debug/Development; Release: nothing.
void dump();                  ///< the current language: code, keys, bytes, chain, unknown keys used
void set_show_keys(bool on);  ///< tr() returns key names (also NNH_LOC_SHOW_KEYS=1 at start, PC)

} // namespace loc
} // namespace nnh
