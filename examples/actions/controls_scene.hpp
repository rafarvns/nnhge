#pragma once
// The controls screen (spec 017 AC-7), pushed by `pause`: lists every play action with its controls; A rebinds the
// selected one (listen; a conflict is swapped and shown), X resets it, Y resets everything, B saves and leaves.
// Two more items: "Reload bindings" (loads the file again and logs a checksum) and "Bench". Its own `menu` context
// is bound to it, so the play actions are off while it is on top.

#include "shared.hpp"

class ControlsScene final : public nnh::Scene {
public:
    explicit ControlsScene(Shared& shared) : s_(shared) {}

    void on_enter() override;
    void update() override;
    void draw() override;

private:
    static constexpr int kItems = 6; ///< 4 actions, reload, bench

    void confirm();
    void leave();
    void reload();
    void finish_listen();

    Shared& s_;
    int cursor_ = 0;
    bool listening_ = false;
    char last_[96] = {};
};
