#pragma once
// The bench's options screen (spec 028 AC-6): two 9-slice panels (the art's frame) — a header on the top screen, a list
// of 12 option rows (about 350 glyphs) on the bottom — built as a UI, and the same gfx calls written out by hand from
// the UI's queried rectangles at setup. tests/ui_example's equivalence test proves both record the same items, so the
// bench compares the UI's own cost per draw with a hand-written menu's.

#include <nnh/texture.hpp>
#include <nnh/ui.hpp>

class OptionsScreen {
public:
    static constexpr int kRows = 12;

    /// Builds the UI (art on texture slot 0). False when out of memory.
    bool build(const nnh::Texture* art);
    nnh::Ui& ui() { return ui_; }

    /// The same draws as ui().draw(), written by hand (positions taken from the UI once, at setup).
    void draw_by_hand() const;
    void capture(); ///< takes the rectangles draw_by_hand uses (after build, before timing)

private:
    nnh::Ui ui_;
    const nnh::Texture* art_ = nullptr;
    nnh::UiWidget header_, title_, panel_, rows_[kRows];
    nnh::UiRect header_r_{}, title_r_{}, panel_r_{}, rows_r_[kRows]{};
};

/// The row texts ("Option 1: music volume 7", ...).
const char* options_row_text(int i);
