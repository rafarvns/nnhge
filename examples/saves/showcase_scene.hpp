#pragma once
// The saves example's showcase (spec 027 AC-5): the tiny game on the top screen with the last load report,
// the slots and settings panel on the bottom one. Buttons: L/R pick a slot, X stores the game in it, A loads
// it, B deletes it; Y runs the bench, SELECT the check, START quits. Touch: every panel button. Log lines
// (examples/saves/tests/check.nnhs waits for them): "saves: showcase slots ...", "slot <n>: stored ...",
// "slot <n>: loaded ...", "deleted", "damaged", "selected", "settings: ...", "golden v1: ...", "name: ...".

#include "demos.hpp"
#include "settings.hpp"
#include "slot_files.hpp"
#include "slots_panel.hpp"
#include "tiny_game.hpp"

#include <nnh/saves.hpp>

class ShowcaseScene final : public nnh::Scene {
  public:
    explicit ShowcaseScene(const Demos& demos);

    void on_enter() override;
    void update() override;
    void draw() override;

  private:
    void refresh_slots();
    void act(saves_example::PanelHit hit);
    void store(int slot);
    void load(int slot);
    void remove(int slot);
    void damage(int slot);
    void load_golden();
    void change_settings(int volume_delta, bool next_language);
    void select(int slot);
    void play_shot();

    const Demos& demos_;
    saves_example::TinyGame game_;
    nnh::save::Buffer<saves_example::Progress, 256> buffer_; ///< + headroom for files of a later version
    saves_example::SlotView slots_[saves_example::kSlots];
    saves_example::Settings settings_;
    int selected_ = 0;
    int frame_ = 0;
    char report_[96] = {};
};
