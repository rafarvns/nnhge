#pragma once
// The saves example's check (spec 027 AC-6): one step per frame, each logging one line the console script
// expects — the golden v1, v2, v3 files copied from the assets to the data folder (the SD card on the 3DS)
// and loaded, the reference encoded, a slot stored twice and reloaded, damaged and loaded from its backup,
// the summaries, a removal — then "saves: PASS" or "saves: FAIL <first mismatch>" against check_golden.hpp
// (recorded from the simulator). SELECT goes back to the showcase; START quits.

#include "demos.hpp"
#include "progress.hpp"

#include <nnh/saves.hpp>

class CheckScene final : public nnh::Scene {
  public:
    explicit CheckScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void update() override;
    void draw() override;

  private:
    void run_step();
    void line(bool ok, const char* format, ...) NNH_PRINTF_FORMAT(3, 4);

    const Demos& demos_;
    saves_example::Progress progress_; ///< what the loads read
    saves_example::Progress stage_;    ///< the reference values stored
    nnh::save::Buffer<saves_example::Progress, 256> buffer_;
    int step_ = 0;
    bool verdict_ = false;
    char failed_[64] = {};
    char lines_[10][72] = {};
};
