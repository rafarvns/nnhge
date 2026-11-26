#pragma once
// What every showcase scene shares: one spec 028 UI bound to the scene, shown in the current language — loc::apply()
// in on_enter() and from a LanguageChanged subscription the scene owns (spec 029 FR-30) — the HUD, SELECT for the next
// language, Y the self-check, X the bench, START quits, and only the top scene drawing.

#include <nnh/events.hpp>
#include <nnh/scene.hpp>
#include <nnh/ui.hpp>

#include "demo.hpp"

class LocScene : public nnh::Scene {
public:
    LocScene(Demo& demo, const char* name) : demo_(demo), name_(name) {}

    void on_enter() override;
    void on_exit() override;
    void update() final;
    void draw() final;

protected:
    bool make_ui(const nnh::UiDesc& desc); ///< the UI, bound to this scene, in the example theme
    virtual void step() = 0;               ///< after the UI's update
    virtual void paint() const {}          ///< the scene's own drawing, under its UI
    virtual void language_changed() {}     ///< after loc::apply (texts re-resolved, fonts set)

    Demo& demo_;
    nnh::Ui ui_;
    const char* name_;

private:
    void on_language(const nnh::LanguageChanged& e);
    nnh::Subscription language_;
};
