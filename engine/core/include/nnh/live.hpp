#pragma once
/// @file nnh/live.hpp
/// Live values (spec 030, D51): tunables the developer changes while the game runs, and watches it shows, on the debug
/// overlay's pages and through `nnh run` (typed commands, `.nnhs` scripts, dumps). Debug and Development builds only:
/// in Release every call is an empty inline, every tunable keeps its declared default and `changed()` is false.
/// Optional (constitution XI): a game that never calls `live::` links none of the module. Guide: docs/api/live.md.

#include <cstddef>
#include <cstdint>

#include "nnh/build.hpp"
#include "nnh/detail/live_kind_of.hpp"
#include "nnh/detail/name_hash.hpp"
#include "nnh/math.hpp"

#include "nnh/detail/live_name.hpp"

namespace nnh::live {

/// A checked name. A string literal is validated and hashed at compile time (a broken rule is a compile error naming
/// it: `live_name_too_long`, `live_name_bad_character`, `live_name_empty_part`, `live_name_too_deep`); a name built at
/// run time goes through `Name::runtime()` and is checked by an assert when it is registered. The registry copies the
/// text, so a run-time buffer may be reused after the call that took the name.
struct Name {
    const char* text = nullptr;
    std::uint32_t hash = 0;     ///< FNV-1a 32 of the text
    std::uint8_t length = 0;    ///< bytes; kMaxNameBytes + 1 marks a run-time name that is too long

    constexpr Name() = default;

    template <std::size_t N>
    consteval Name(const char (&literal)[N]) // NOLINT(google-explicit-constructor): names are written as literals
        : text(literal), hash(detail::fnv1a32(literal, N - 1)), length(static_cast<std::uint8_t>(N - 1)) {
        using detail::live_name::Error;
        switch (detail::live_name::check(literal, N - 1)) {
        case Error::None: break;
        case Error::TooLong: detail::live_name::live_name_too_long(); break;
        case Error::BadCharacter: detail::live_name::live_name_bad_character(); break;
        case Error::EmptyPart: detail::live_name::live_name_empty_part(); break;
        case Error::TooDeep: detail::live_name::live_name_too_deep(); break;
        }
    }

    /// A name from a run-time buffer (`goblin/speed` built from data). Not checked here: registration asserts on a
    /// broken rule. Reads at most kMaxNameBytes + 1 bytes of `text`.
    static constexpr Name runtime(const char* text) {
        Name name;
        name.text = text;
        std::size_t n = 0;
        while (text != nullptr && n <= static_cast<std::size_t>(kMaxNameBytes) && text[n] != '\0') ++n;
        name.length = static_cast<std::uint8_t>(n);
        name.hash = text != nullptr ? detail::fnv1a32(text, n) : 0;
        return name;
    }
};

} // namespace nnh::live

namespace nnh {
struct Color; // <nnh/gfx.hpp>
} // namespace nnh

namespace nnh::live {

/// A float tunable's range: required, min < max, step > 0, at most 2^24 steps. Values move on the grid
/// min + k·step (never drifting) and stop at min and max.
struct FloatRange {
    float min = 0, max = 0, step = 0;
};
/// An int tunable's range: required, min < max, step > 0, at most 2^24 steps.
struct IntRange {
    std::int32_t min = 0, max = 0, step = 1;
};
/// Called at the application point (the start of the frame) after a panel or `nnh` change wrote different bits; never
/// for the game's own reset(). The alternative to polling changed() (spec 030 Q6).
struct OnChange {
    void (*fn)(void* context) = nullptr;
    void* context = nullptr;
};

} // namespace nnh::live

namespace nnh::detail::live_api {

struct Adopt {};
// Back ends (src/live/live_tune.cpp, live_watch.cpp): Debug/Development only, reached through the inline calls below.
std::uint32_t tune_float(const nnh::live::Name& name, float* value, const nnh::live::FloatRange& range,
                         nnh::live::OnChange on_change);
std::uint32_t tune_int(const nnh::live::Name& name, void* value, const nnh::live::IntRange& range,
                       nnh::live::OnChange on_change);
std::uint32_t tune_bool(const nnh::live::Name& name, bool* value, nnh::live::OnChange on_change);
std::uint32_t tune_color(const nnh::live::Name& name, nnh::Color* value, nnh::live::OnChange on_change);
std::uint32_t tune_enum(const nnh::live::Name& name, void* value, int value_bytes, const char* const* names, int count,
                        nnh::live::OnChange on_change);
std::uint32_t action(const nnh::live::Name& name, void (*fn)(void*), void* context);
std::uint32_t watch(const nnh::live::Name& name, const void* value, std::uint8_t kind, int text_bytes);
void show(const nnh::live::Name& name, std::uint8_t kind, const void* value, int bytes);
void show_text(const nnh::live::Name& name, const char* text);
void remove(std::uint32_t id);
bool changed(std::uint32_t id);
void reset(std::uint32_t id);
void reset_all();
void reset_group(const nnh::live::Name& group);
void configure(int max_tunables, int max_watches, int max_shown);
void pause();
void show_panel(std::uint8_t page, const char* select);

} // namespace nnh::detail::live_api

namespace nnh::live {

/// Owns one entry (a tunable, an action or a registered watch): move-only, 4 bytes in every flavour; its destructor
/// removes the entry from the panel and `nnh` (FR-6). Release: an empty shell (valid() and changed() false).
class Handle {
public:
    Handle() = default;
    Handle(detail::live_api::Adopt, std::uint32_t id) : id_(id) {}
    ~Handle() {
        if constexpr (kEnabled) {
            if (id_ != 0) detail::live_api::remove(id_);
        }
    }
    Handle(Handle&& other) noexcept : id_(other.id_) { other.id_ = 0; }
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            if constexpr (kEnabled) {
                if (id_ != 0) detail::live_api::remove(id_);
            }
            id_ = other.id_;
            other.id_ = 0;
        }
        return *this;
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    /// The entry is registered (false after a move, and in Release).
    bool valid() const {
        if constexpr (kEnabled) return id_ != 0;
        return false;
    }
    /// Tunables: true on exactly the one frame where a panel or `nnh` change wrote different bits (FR-4). Never for
    /// the game's own reset(). A handle without an entry (moved from, or the engine shut down) asserts. Release: false.
    bool changed() const {
        if constexpr (kEnabled) return detail::live_api::changed(id_);
        return false;
    }
    /// Writes the default now (game code: no changed(), no callback). A handle without an entry asserts. Release:
    /// nothing (the variable already holds its default).
    void reset() {
        if constexpr (kEnabled) detail::live_api::reset(id_);
    }
    /// The id (slot + generation; 0 for none), for tests.
    std::uint32_t id() const { return id_; }

private:
    std::uint32_t id_ = 0;
};

/// Registers a float tunable. Asserts on a name used twice or equal to a group path, a full table, a bad range, a
/// default (the variable's value now) outside the range, a null pointer. Allocates the registry block at the first
/// registration of the game (once); never after. Not per frame: call it when the owner starts (a scene's on_enter).
[[nodiscard]] inline Handle tune(Name name, float* value, FloatRange range, OnChange on_change = {}) {
    if constexpr (kEnabled) {
        return Handle(detail::live_api::Adopt{}, detail::live_api::tune_float(name, value, range, on_change));
    }
    return {};
}
/// An int tunable: a 32-bit signed `int` (or `long` where it has 32 bits; std::int32_t either way). Same rules.
template <typename I>
    requires(detail::live_api::is_int32<I>())
[[nodiscard]] inline Handle tune(Name name, I* value, IntRange range, OnChange on_change = {}) {
    if constexpr (kEnabled) {
        return Handle(detail::live_api::Adopt{}, detail::live_api::tune_int(name, value, range, on_change));
    }
    return {};
}
/// A bool tunable: left, right and A toggle it.
[[nodiscard]] inline Handle tune(Name name, bool* value, OnChange on_change = {}) {
    if constexpr (kEnabled) return Handle(detail::live_api::Adopt{}, detail::live_api::tune_bool(name, value, on_change));
    return {};
}
/// A colour tunable: four channels 0..255 (A cycles the edited channel on the panel).
[[nodiscard]] inline Handle tune(Name name, Color* value, OnChange on_change = {}) {
    if constexpr (kEnabled) return Handle(detail::live_api::Adopt{}, detail::live_api::tune_color(name, value, on_change));
    return {};
}
/// An enum tunable shown by name: the game's enum type (any underlying size) with values 0..N−1 named by `names`
/// (1..32 names, 1..15 bytes each; the array must outlive the handle).
template <typename E, std::size_t N>
    requires(__is_enum(E))
[[nodiscard]] inline Handle tune(Name name, E* value, const char* const (&names)[N], OnChange on_change = {}) {
    static_assert(N >= 1 && N <= static_cast<std::size_t>(kMaxEnumNames), "an enum tunable has 1..32 names");
    if constexpr (kEnabled) {
        return Handle(detail::live_api::Adopt{}, detail::live_api::tune_enum(name, value, static_cast<int>(sizeof(E)),
                                                                             names, static_cast<int>(N), on_change));
    }
    return {};
}
/// An action: a panel row (A, or a tap on `run`) and the `action NAME` command call `fn(context)` at the application
/// point (the start of the next frame), once per press.
[[nodiscard]] inline Handle action(Name name, void (*fn)(void* context), void* context = nullptr) {
    if constexpr (kEnabled) return Handle(detail::live_api::Adopt{}, detail::live_api::action(name, fn, context));
    return {};
}

/// A registered watch: the engine reads `*value` only on frames where it is shown on the Watches page or followed by
/// `nnh` (at the end of the frame). Kinds: bool, 8/16/32-bit signed and unsigned integers, float, Vec2, Color.
template <typename T>
[[nodiscard]] inline Handle watch(Name name, const T* value) {
    static_assert(detail::live_api::kind_of<T>() != 0, "watch: bool, int8..int32, uint8..uint32, float, Vec2 or Color");
    if constexpr (kEnabled) {
        return Handle(detail::live_api::Adopt{}, detail::live_api::watch(name, value, detail::live_api::kind_of<T>(), 0));
    }
    return {};
}
/// A text watch through a pointer the game updates (`const char* state = "idle"; watch("player/state", &state)`); at
/// most 31 bytes are read.
[[nodiscard]] inline Handle watch(Name name, const char* const* text) {
    if constexpr (kEnabled) return Handle(detail::live_api::Adopt{}, detail::live_api::watch(name, text, 13, -1));
    return {};
}
/// A text watch of a char array (up to its NUL, at most 31 bytes).
template <std::size_t N>
[[nodiscard]] inline Handle watch(Name name, const char (&text)[N]) {
    if constexpr (kEnabled) {
        return Handle(detail::live_api::Adopt{}, detail::live_api::watch(name, text, 13, static_cast<int>(N)));
    }
    return {};
}

/// A per-frame watch: the last value given this frame, listed while it was shown in the last 120 frames. No handle,
/// no lifetime. More names than live::Config::max_shown asserts. Arguments are evaluated even in Release (like any
/// call): test live::kEnabled to skip costly ones.
template <typename T>
inline void show(Name name, T value) {
    static_assert(detail::live_api::kind_of<T>() != 0, "show: bool, int8..int32, uint8..uint32, float, Vec2 or Color");
    if constexpr (kEnabled) {
        detail::live_api::show(name, detail::live_api::kind_of<T>(), &value, static_cast<int>(sizeof(T)));
    }
}
/// A per-frame text watch: copied (at most 31 bytes; a longer text is cut and warned once).
inline void show(Name name, const char* text) {
    if constexpr (kEnabled) detail::live_api::show_text(name, text);
}

/// Every tunable to its default, now (game code: no changed(), no callback).
inline void reset_all() {
    if constexpr (kEnabled) detail::live_api::reset_all();
}
/// Every tunable of `group` (`"player"`, `"fx/spark"`) to its default, now.
inline void reset_group(Name group) {
    if constexpr (kEnabled) detail::live_api::reset_group(group);
}

/// Capacities of the registry (one block, tag `live`, allocated at the first registration).
struct Config {
    int max_tunables = 128; ///< tunables and actions
    int max_watches = 64;   ///< registered watches
    int max_shown = 64;     ///< per-frame watches
};
/// Sets the capacities. Only before the first registration (asserts otherwise).
inline void configure(const Config& config) {
    if constexpr (kEnabled) detail::live_api::configure(config.max_tunables, config.max_watches, config.max_shown);
}

/// Pauses the game at the start of the next frame (D52): nnh::frame() then waits, the last frame stays on screen and
/// the overlay stays live; the developer steps (SELECT + X, `step`) or resumes (SELECT + Y, `resume`).
inline void pause() {
    if constexpr (kEnabled) detail::live_api::pause();
}

/// The debug overlay's pages (SELECT + R cycles them while the overlay is shown).
enum class Page : std::uint8_t { Stats, Tunables, Watches };
/// Shows the overlay on `page`, with the row named `select` selected (its groups opened) when given.
inline void show_panel(Page page, const char* select = nullptr) {
    if constexpr (kEnabled) detail::live_api::show_panel(static_cast<std::uint8_t>(page), select);
}

} // namespace nnh::live
