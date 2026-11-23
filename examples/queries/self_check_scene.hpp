#pragma once
// Plays the self-check (self_check.hpp), 64 rays or casts per frame: logs "queries: <n> sum ..." every 1024 rays
// and the verdict "queries: PASS <sum>" or "queries: FAIL ..." (examples/queries/tests/check.nnhs waits for them).
// SELECT goes to the bench; START quits.

#include "demos.hpp"
#include "self_check.hpp"

#include <memory>
#include <nnh/scene.hpp>

class SelfCheckScene final : public nnh::Scene {
  public:
    static constexpr int kPerFrame = 64;

    explicit SelfCheckScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override { check_.reset(); }
    void update() override;
    void draw() override;

  private:
    const Demos& demos_;
    std::unique_ptr<QueriesSelfCheck> check_; // made on enter (the script starts again each time), on the heap
    char last_line_[64] = {};
    char verdict_[80] = {};
};
