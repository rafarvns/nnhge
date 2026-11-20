#pragma once
// NNH Link's only scene: buttons and touches → LinkState, LinkState → the views. It pops itself (ending
// nnh::run) when LinkState says to quit.

#include <nnh/scene.hpp>

#include "link_state.hpp"
#include "pad.hpp"
#include "storage.hpp"

namespace link_app {

class LinkScene final : public nnh::Scene {
public:
    /// `persist`: save the storage when it changes (false for the demo).
    LinkScene(LinkState& link, Storage& storage, bool persist) : link_(link), storage_(storage), persist_(persist) {}

    void on_enter() override;
    void update() override;
    void draw() override;

private:
    void pad_input();
    void list_input();
    void press_pad_key(char c);

    LinkState& link_;
    Storage& storage_;
    bool persist_;
    Pad pad_{false};
    Screen pad_screen_ = Screen::Servers; ///< the screen the pad was set up for
};

} // namespace link_app
