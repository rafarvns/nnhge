#pragma once
// Y (or LOC_DEMO=check): the self-check (spec 029 AC-2, AC-4). Each language is switched to with set_language, like a
// player would; in the next frame (the switch done, its fonts loaded) its sum is folded in and compared with its
// checkpoint: "loc: <code> sum <S>", then "loc: PASS <G>" or "loc: FAIL …". The language before the check comes back.

#include <cstdint>
#include <nnh/scene.hpp>

#include "demo.hpp"

class CheckScene final : public nnh::Scene {
public:
    explicit CheckScene(Demo& demo) : demo_(demo) {}
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    Demo& demo_;
    int step_ = 0;
    int linger_ = 0;
    std::uint32_t hash_ = 0;
    bool failed_ = false;
    char previous_[16] = {};
    char line_[96] = {};
    char verdict_[96] = {};
};
