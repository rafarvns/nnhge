#pragma once
// The game's one scene: a sprite moved by the Circle Pad or the D-Pad on the top screen, a sound on A, text
// on both screens, START quits. Its state lives here (a scene owns what it uses); input is queried every
// frame; the end of a sound arrives as an event. NNHGE docs: api/scene.md.

#include <nnh/audio.hpp>
#include <nnh/events.hpp>
#include <nnh/math.hpp>
#include <nnh/scene.hpp>
#include <nnh/texture.hpp>

class PlayScene final : public nnh::Scene {
  public:
    void on_enter() override; // loads the assets, subscribes to events
    void on_exit() override;  // gives them back
    void update() override;   // changes state, 60 times a second
    void draw() override;     // only reads state

  private:
    void move_player();
    void on_sound_finished(const nnh::SoundFinished& finished);

    nnh::Texture player_;          // assets/art/player.png (api/texture.md)
    nnh::Sound blip_;              // assets/sfx/blip.wav (api/audio.md)
    nnh::Voice blip_voice_;        // the blip playing now, if any
    nnh::Subscription sound_done_; // SoundFinished while the scene runs (api/events.md)
    nnh::Vec2 position_{184, 104}; // the sprite's top-left corner, in top-screen pixels
    bool started_ = false;
    bool moving_ = false;
    int blips_finished_ = 0;
    int bye_frames_ = 0; // > 0: quitting, "Bye!" shown for this many more frames
};
