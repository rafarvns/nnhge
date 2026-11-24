#pragma once
// Logs every input the game sees (`input: <button>`, `input: touch x,y`) and counts the ones seen while a
// transition runs (spec 023 AC-6: must stay 0 — the transition blocks input).

#include <nnh/events.hpp>
#include <nnh/input.hpp>

class InputLog {
public:
    /// Subscribes to the input events (call after nnh::init()).
    void attach();
    void detach();
    /// A scene's look at the input queries (from its draw, which runs every frame): counts it if a transition runs
    /// and a control reads held.
    void look();
    int seen() const { return seen_; }
    void reset_seen() { seen_ = 0; }

private:
    void on_button(const nnh::ButtonPressed& e);
    void on_touch(const nnh::TouchStarted& e);

    nnh::Subscription buttons_;
    nnh::Subscription touches_;
    int seen_ = 0;
};

const char* button_name(nnh::Button button);
