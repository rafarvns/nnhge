#pragma once
// The UI self-check (spec 028 AC-2): one UI with both roots — a title menu (a disabled entry, an inert one, a quit
// prompt), an options scroll box (two sliders, a toggle, a wrapping and a non-wrapping choice, a disabled entry, Back),
// a 1000-item list (every 7th row disabled), a 6-column grid of 64, a revealed dialog, a top-root HUD with explicit
// neighbours between the roots — driven by a fixed 3600-frame script of directions with holds, confirms, cancels,
// pages, touches and drags (kUiScript), with an item-count change at 2000, a language switch at 2400 and a rebuilt
// title restoring its focus by key at 3000. A running FNV-1a folds every frame's results and, every 10 frames, the
// draw hash; every 600 frames a checkpoint folds every live widget. Pure: compiled into the example and nnh_tests.

#include <cstdint>
#include <nnh/ui.hpp>

namespace ui_check {

inline constexpr int kFrames = 3600;
inline constexpr int kCheckpointEvery = 600;
inline constexpr int kCheckpoints = kFrames / kCheckpointEvery;

/// Golden values (recorded from pc-debug; every host compiler and the 3DS must give them).
extern const std::uint32_t kUiCheckpoints[kCheckpoints];
extern const std::uint32_t kUiCheckGolden;

/// The script's input for `frame` (0..kFrames-1). self_check_script.cpp.
nnh::UiInput script_input(int frame);

class SelfCheck {
public:
    bool begin();          ///< creates the UI; false when out of memory
    bool step();           ///< one script frame; true when it ended a checkpoint
    bool done() const { return frame_ >= kFrames; }
    int frame() const { return frame_; }
    std::uint32_t checksum() const { return hash_; }
    const nnh::Ui& ui() const { return ui_; }
    /// What the script reached: bits 0-4 the screens shown (title, options, list, grid, dialog), 5 a prompt opened,
    /// 6 its Yes answered, 7 a refused, 8 a blocked move, 9 a page, 10 a cancel, 11 a value change by touch.
    int reached() const { return reached_; }

private:
    void build_title();
    void build_rest();
    void show(int screen);
    void react();
    void fold(std::uint32_t word);
    nnh::UiWidget keep(nnh::UiWidget w); ///< remembered for the checkpoints
    void checkpoint();

    nnh::Ui ui_;
    nnh::UiWidget title_, options_, list_panel_, list_, grid_, dialog_, dialog_text_, info_, health_, score_;
    nnh::UiWidget prompt_;
    static constexpr int kMade = 96;
    nnh::UiWidget made_[kMade];
    int made_count_ = 0;
    char score_text_[16] = "0";
    int screen_ = 0; ///< 0 title, 1 options, 2 list, 3 grid, 4 dialog
    int frame_ = 0;
    int quits_ = 0;
    int reached_ = 0;
    std::uint32_t hash_ = 2166136261u;
};

} // namespace ui_check
