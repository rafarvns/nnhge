#pragma once
// X (or LOC_DEMO=bench): the bench (spec 029 AC-6; plan "Self-check and bench"). Phases of 300 frames (the frame test
// 600, the switch 20 switches), each timing only its own work: base (no text: the submit baseline), builtin400 and
// font400 (400 glyphs, built-in vs the body font: CPU + submit per glyph over base), frame (font400 + 300 sprites),
// measure_latin and measure_ja (400-character paragraphs wrapped at 300 px, nothing drawn), tr1000, format100, switch
// (en → ja → en, timed inside the frame hook), page1024 (a font with one 1024 × 1024 page loaded and released), then the
// memory line and "loc bench: PASS" or "loc bench: FAIL (names)". Debug builds log "not judged (Debug)".

#include <cstdint>
#include <nnh/font.hpp>
#include <nnh/scene.hpp>
#include <nnh/texture.hpp>

#include "demo.hpp"

class BenchScene final : public nnh::Scene {
public:
    enum Phase { kBase, kBuiltin, kFont, kFrame, kMeasureLatin, kMeasureJa, kTr, kFormat, kSwitch, kPage, kPhases };

    explicit BenchScene(Demo& demo) : demo_(demo) {}
    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    struct Sum {
        double own = 0, own_worst = 0, submit = 0;
        int n = 0;
    };
    void run_phase(double& ms);
    void finish();
    void verdict();
    double per_frame(Phase p) const;

    Demo& demo_;
    Sum sums_[kPhases]{};
    int phase_ = 0, frame_ = 0, missed_ = 0, glyphs_ = 0;
    double worst_frame_ = 0;
    bool done_ = false, memory_logged_ = false;
    char previous_[16] = {};
    std::uint32_t keys_[1000] = {};
    char failed_[128] = {};
    nnh::Font ja_;
    nnh::Texture sprite_;
};
