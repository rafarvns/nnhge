#pragma once
// The live registry's block (spec 030 plan, "The registry", "Data and memory"): one allocation (tag `live`) at the first
// registration, holding fixed-size entries with copied names, two indexes sorted by name (tunables and actions; watches
// registered and per-frame), the per-frame watches' hash table, the follow table, the remote operation queue and the
// panel's state. Internal to src/live/.

#include <cstddef>
#include <cstdint>

#include "live/live_kinds.hpp"
#include "nnh/live.hpp"

namespace nnh::detail::live {

inline constexpr int kNameBytes = 32; ///< a copied name and its NUL
inline constexpr int kMaxFollows = 16;
inline constexpr int kMaxOps = 64;
inline constexpr int kShownForFrames = 120; ///< a per-frame watch not shown for this long is listed no more
inline constexpr int kSweepEvery = 60;
inline constexpr std::uint16_t kShownRef = 0x8000; ///< in the watch index: a per-frame watch's slot

// Entry flags
inline constexpr std::uint8_t kUsed = 1u << 0;
inline constexpr std::uint8_t kPending = 1u << 1;    ///< a panel or remote value waits for the application point
inline constexpr std::uint8_t kChanged = 1u << 2;    ///< changed_frame is meaningful
inline constexpr std::uint8_t kRunPending = 1u << 3; ///< an action waits for the application point
inline constexpr std::uint8_t kTextPointer = 1u << 4; ///< a text watch through `const char* const*`

/// A tunable or an action (≈ 80 bytes on the 3DS).
struct Entry {
    void* target = nullptr;              ///< the game's variable (actions: unused)
    void (*callback)(void*) = nullptr;   ///< OnChange, or the action's function
    void* context = nullptr;
    const char* const* enum_names = nullptr;
    std::uint32_t hash = 0;
    std::uint32_t def = 0, min = 0, max = 0, step = 0; ///< bits (floats as bits, ints, colours, enum count in max)
    std::uint32_t pending = 0;
    std::uint32_t changed_frame = 0;
    std::uint16_t generation = 0;
    Kind kind = Kind::None;
    std::uint8_t flags = 0;
    std::uint8_t value_bytes = 4; ///< enums: the enum's size (1, 2, 4)
    std::uint8_t enum_count = 0;
    std::uint8_t name_length = 0;
    char name[kNameBytes] = {};
};

/// A registered watch (≈ 48 bytes on the 3DS).
struct WatchEntry {
    const void* target = nullptr;
    std::uint32_t hash = 0;
    std::int16_t text_bytes = 0; ///< text arrays: the array's size
    std::uint16_t generation = 0;
    Kind kind = Kind::None;
    std::uint8_t flags = 0;
    std::uint8_t name_length = 0;
    char name[kNameBytes] = {};
};

/// A per-frame watch (80 bytes).
struct ShownEntry {
    std::uint32_t hash = 0;
    std::uint32_t last_frame = 0;
    Kind kind = Kind::None;
    std::uint8_t flags = 0;
    std::uint8_t length = 0; ///< value bytes
    std::uint8_t name_length = 0;
    std::uint8_t value[32] = {};
    char name[kNameBytes] = {};
};

/// A followed value: a LiveValue record every `every` frames, `left` times (0: until unfollowed).
struct Follow {
    std::uint32_t hash = 0;
    std::uint32_t next_frame = 0;
    std::uint16_t left = 0;
    std::uint8_t every = 0;
    std::uint8_t once = 0; ///< a Get: one answer
};

/// A remote operation waiting for the application point (12 bytes).
struct RemoteOp {
    std::uint8_t op = 0; ///< remote::LiveOp, or kOpSet
    std::uint8_t arg = 0;
    std::uint16_t seq = 0;
    std::uint32_t hash = 0;
    std::uint32_t bits = 0;
};
inline constexpr std::uint8_t kOpSet = 0x40;

/// The panel's state (src/live/live_panel_model.cpp).
struct PanelState {
    std::uint32_t open_groups[16] = {}; ///< hashes of opened group paths
    int open_count = 0;
    int selected = 0; ///< row
    int scroll = 0;   ///< first visible row
    int channel = 0;  ///< a colour row's edited channel
    int watch_selected = 0;
    int watch_scroll = 0;
    std::uint8_t touch_mode = 0; ///< live_panel_touch.cpp: none, ignored, drag, minus, plus
    std::uint16_t touch_slot = 0;
    int touch_ticks = 0;
};

struct Registry {
    int max_tunables = 0, max_watches = 0, max_shown = 0;
    int tunable_count = 0, watch_count = 0, shown_count = 0;
    Entry* entries = nullptr;
    WatchEntry* watches = nullptr;
    ShownEntry* shown = nullptr;
    std::uint16_t* tunable_index = nullptr; ///< used slots sorted by name
    std::uint16_t* watch_index = nullptr;   ///< watch slots and kShownRef | shown slots, sorted by name
    std::uint16_t* shown_table = nullptr;   ///< open addressing, 2 × max_shown, slot + 1 (0: empty)
    Follow follows[kMaxFollows];
    int follow_count = 0;
    RemoteOp ops[kMaxOps];
    int op_count = 0;
    int pending_count = 0; ///< entries with kPending or kRunPending
    bool dump_requested = false;
    bool text_cut_warned = false;
    std::uint32_t next_sweep = 0;
    std::uint32_t walks = 0;    ///< walks over the registry (tests: none while nothing happens, FR-19)
    std::uint32_t refused = 0;  ///< remote operations refused
    std::uint32_t refused_by_reason[8] = {};
    PanelState panel;
};

/// The registry, or null before the first registration (and after shutdown).
extern Registry* g_registry;

/// Bytes of the block for these capacities (header and every table).
std::size_t block_bytes(int max_tunables, int max_watches, int max_shown);

} // namespace nnh::detail::live
