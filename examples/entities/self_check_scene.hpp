#pragma once
// Plays the self-check (self_check.hpp), 2000 operations per frame: logs "entities: <n> sum ..." every 10 000 and the
// verdict "entities: PASS <sum>" or "entities: FAIL ..." (examples/entities/tests/check.nnhs waits for them).
// SELECT goes back to the showcase; START quits.

#include "demos.hpp"
#include "self_check.hpp"

#include <memory>
#include <nnh/scene.hpp>

class SelfCheckScene final : public nnh::Scene {
  public:
    static constexpr int kPerFrame = 2000;

    explicit SelfCheckScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override { check_.reset(); }
    void update() override;
    void draw() override;

  private:
    const Demos& demos_;
    std::unique_ptr<EntitiesSelfCheck> check_; // made on enter (the script starts again each time), on the heap
    char last_line_[64] = {};
    char verdict_[80] = {};
};
