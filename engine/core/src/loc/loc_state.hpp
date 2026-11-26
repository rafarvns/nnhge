#pragma once
// Localization's module state (spec 029 D50): the manifest, the block holding the current language's chain of tables,
// the pending switch, following the system language, the fonts hook. Internal to src/loc/.

#include <cstddef>
#include <cstdint>

#include "core/input_hooks.hpp"
#include "loc/loc_file.hpp"
#include "nnh/localization.hpp"

namespace nnh::detail::loc {

struct LoadedTable {
    const std::uint8_t* base = nullptr;
    TableHeader h{};
    const std::uint16_t* buckets = nullptr;
    const KeyRecord* keys = nullptr;
    const StringRecord* strings = nullptr;
    const PlaceholderRecord* placeholders = nullptr;
    const char* text = nullptr;
};

struct LocState {
    bool started = false;
    MemTag* tag = nullptr;
    std::uint8_t* manifest = nullptr;
    ManifestHeader header{};
    const LanguageRecord* languages = nullptr;
    std::uint8_t* block = nullptr;
    std::size_t block_bytes = 0;
    LoadedTable tables[kMaxChain]{};
    int table_count = 0;
    int current = -1;
    std::uint32_t serial = 0;
    int pending = -1;
    const char* pending_why = "player";
    bool follow = false;
    char system_code[16] = {};
    InputHook hook{};
    bool hook_added = false;
    bool show_keys = false;
    /// Set by use_fonts() (loc_fonts.cpp): loads the role fonts of a language; nullptr while fonts aren't used.
    void (*fonts_switch)(int language) = nullptr;
};

/// The one state (loc_state.cpp); inline so tr() and format() reach it without a call (T10.3).
extern LocState g_loc_state;
inline LocState& loc_state() { return g_loc_state; }

/// Loads `language`'s chain into the block. @return nullptr or the reason (the block then holds garbage: reload).
const char* load_chain(int language);
/// The language a system code matches (exact, then the same language and script with another region), or −1.
int match_language(const char* code);
/// The after-input hook (loc_switch.cpp): applies the pending switch, follows the system language.
void loc_frame_hook(InputHook& self);
/// Logs and publishes a switch that took effect.
void announce(int previous, int current, const char* why);
/// The text blob of the manifest.
inline const char* blob(std::uint32_t offset) {
    const LocState& s = loc_state();
    return reinterpret_cast<const char*>(s.manifest + s.header.off_blob + offset);
}

} // namespace nnh::detail::loc
