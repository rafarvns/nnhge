#pragma once
// The live panel's model (spec 030 FR-5, FR-8, Q9; plan "Panel"): rows from the sorted indexes (group rows open and
// close), selection, scroll, buttons → pending edits, colour channels, enums, actions, resets and dump requests. Pure
// over the registry and an input value. Tests: tests/live/test_live_panel_model.cpp.

#include <cstdint>

#include "live/live_block.hpp"

namespace nnh::detail {
struct DevInput;
}

namespace nnh::detail::live {

inline constexpr int kPageTunables = 1;
inline constexpr int kPageWatches = 2;
inline constexpr int kTunableRowsVisible = 9;
inline constexpr int kWatchRowsVisible = 17;

enum class RowType : std::uint8_t { Group, Tunable, Watch, Shown };

/// One visible row: a group (its path is the first `length` bytes of `name`), or an entry.
struct Row {
    RowType type = RowType::Group;
    std::uint8_t depth = 0;    ///< indentation (groups above it)
    std::uint8_t length = 0;   ///< bytes of the group path, or of the name
    bool open = false;         ///< groups
    std::uint16_t ref = 0;     ///< slot (Shown: shown slot)
    std::uint16_t count = 0;   ///< groups: entries inside, at any depth
    const char* name = nullptr; ///< the full name of the entry or of the group's first entry
};

/// Rows are generated from the sorted index on demand (nothing stored): their count, and row `index`.
int row_count(const Registry& r, int page);
bool row_at(const Registry& r, int page, int index, Row& out);

/// The text shown for a row: the leaf of an entry, the last part of a group path. Not NUL-terminated.
const char* row_label(const Row& row, int& length);

/// Buttons on a page (the engine controls of this tick). Edits become pending values (set_pending); `paused`
/// doesn't change what a press does (the edits wait until the next game frame either way).
void panel_buttons(Registry& r, int page, const DevInput& in);

/// The row selected on `page` (clamped to the rows there are), and the first visible one.
int panel_selected(const Registry& r, int page);
int panel_scroll(const Registry& r, int page);

/// Selects the entry named `name` on `page`, opening its groups (live::show_panel).
void panel_select_name(Registry& r, int page, const char* name);

/// The value an edit starts from: the pending value while one waits, else the variable's.
std::uint32_t edit_base(const Entry& e);

/// One step (±1, ±10) of an entry: a float or int on its grid, a bool toggled, a colour's `channel`, an enum's index.
std::uint32_t stepped(const Entry& e, std::uint32_t bits, int delta, int channel);

/// Touch on the Tunables page (live_panel_touch.cpp).
void panel_touch(Registry& r, const DevInput& in);

/// Draws page 1 or 2 into the debug pass (live_panel_draw.cpp).
void panel_draw_page(const Registry& r, int page, std::uint32_t frame, bool paused);

/// Whether a group path (hash) is open.
bool group_open(const PanelState& p, std::uint32_t hash);
void toggle_group(PanelState& p, std::uint32_t hash);

} // namespace nnh::detail::live
