#pragma once
// What every showcase scene shares: one UI as a member bound to the scene (D30: updated right before update(), its
// focus memory kept across pushes and pops with no code), the HUD and background, SELECT switching the theme, Y the
// self-check and X the bench, the language, one log line per result, and only the top scene drawing.

#include <cstdint>
#include <nnh/scene.hpp>
#include <nnh/ui.hpp>

#include "demos.hpp"

class MenuScene : public nnh::Scene {
public:
    MenuScene(Demo& demo, const char* screen) : demo_(demo), screen_(screen) {}

    void update() final;
    void draw() final;

protected:
    /// Creates the UI (bound to this scene, the current look and language). False when out of memory.
    bool make_ui(const nnh::UiDesc& desc);
    virtual void step() = 0;           ///< after the UI's update and the results' log lines
    virtual void paint() const {}      ///< the scene's own drawing, under its UI
    virtual void returned() {}         ///< back on top after a scene above it was popped

    Demo& demo_;
    nnh::Ui ui_;
    const char* screen_;

private:
    int theme_seen_ = -1;
    int language_seen_ = -1;
    std::uint64_t last_frame_ = 0;
};
