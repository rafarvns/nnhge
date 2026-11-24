#pragma once
// The bottom screen of the saves example (spec 027 AC-5): three slot rows (y 8-63, 72-127, 136-191), each
// with its summary or "empty" and four touch buttons — Save (x 8-79), Load (88-159), Delete (168-239), Damage
// (248-311) — and a bottom row (y 200-231): Volume - (8-47), Volume + (56-95), Language (104-175), Load
// golden v1 (184-255), Rename (264-311). The touch areas are whole columns of a row.

#include "summary.hpp"

#include <cstdint>

namespace saves_example {

enum class PanelAction : std::uint8_t {
    None,
    Save,
    Load,
    Delete,
    Damage,
    VolumeDown,
    VolumeUp,
    Language,
    GoldenV1,
    Rename,
};

struct PanelHit {
    PanelAction action = PanelAction::None;
    int slot = 0; ///< for the slot actions
};

/// What a touch at (x, y) asks for.
PanelHit panel_hit(int x, int y);

/// What a slot row shows.
struct SlotView {
    bool used = false;
    bool from_backup = false;
    SlotSummary summary;
};

/// Draws the panel: the rows (`selected` highlighted) and the buttons.
void draw_panel(const SlotView (&slots)[3], int selected);

/// "L12" or "empty": the showcase's log line and the check mode's summaries line.
void format_slot_short(char* out, std::size_t capacity, const SlotView& slot);

} // namespace saves_example
