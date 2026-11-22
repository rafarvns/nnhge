#include "hero.hpp"

#include <cstdio>
#include <cstdlib>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

#include "demos.hpp"

namespace {

constexpr float kGround = 200;

} // namespace

bool Hero::load() {
    auto texture = nnh::load_texture("art/hero");
    // The export has clips and markers; the state machine is added in code, in the room asked for here (FR-18).
    auto library = nnh::load_sprite_library("art/hero", {.extra_states = 6, .extra_transitions = 3});
    if (!texture || !library) {
        NNH_LOG_ERROR(anim, "hero: %s", !texture ? texture.error().message : library.error().message);
        return false;
    }
    texture_ = std::move(texture.value());
    library_ = std::move(library.value());
    if (!add_knight_states(library_)) {
        NNH_LOG_ERROR(anim, "hero: the loaded library lacks a knight clip");
        return false;
    }
    auto sound = nnh::load_sound("sfx/step");
    if (sound) step_ = std::move(sound.value());
    states_ = KnightStates{library_};
    anim_ = nnh::SpriteAnimator{library_, states_.idle};
    anim_.on_notice([this](nnh::SpriteAnimator&, const nnh::SpriteNotice& n) { on_notice(n); });
    const char* run = std::getenv("HERO_DEMO_RUN");
    auto_run_ = run != nullptr && run[0] == '1';
    NNH_LOG_INFO(anim, "hero: %d frames, %d clips, %d states, %u bytes; sizeof(SpriteAnimator) = %u",
                 library_.frame_count(), library_.clip_count(), library_.state_count(),
                 static_cast<unsigned>(library_.memory_used()), static_cast<unsigned>(sizeof(nnh::SpriteAnimator)));
    return true;
}

void Hero::unload() {
    anim_ = nnh::SpriteAnimator{};
    library_ = nnh::SpriteLibrary{};
    texture_.reset();
    step_.reset();
}

void Hero::on_notice(const nnh::SpriteNotice& n) {
    if (n.kind != nnh::SpriteNoticeKind::Marker) return;
    if (n.marker == nnh::sprite_name("step")) {
        dust_ = 8;
        ++steps_;
        (void)nnh::audio::play(step_, {.volume = 0.6f, .pan = (x_ - 160) / 160});
    } else if (n.marker == nnh::sprite_name("hit")) {
        hit_ = 6 * n.value; // the marker's value: how long the hit box shows
    }
}

void Hero::update() {
    using nnh::Button;
    const bool left = nnh::input::held(Button::Left), right = nnh::input::held(Button::Right) || auto_run_;
    const bool moving = left != right;
    if (moving) facing_left_ = left;
    const bool attacking = anim_.state() == states_.attack || anim_.state() == states_.hurt;
    if (moving && !attacking) x_ += facing_left_ ? -2.0f : 2.0f;
    if (x_ < 16) x_ = 304;
    if (x_ > 304) x_ = 16;
    const bool on_ground = y_ >= kGround;
    if (on_ground && nnh::input::pressed(Button::A)) vy_ = -6;
    vy_ += 0.35f;
    y_ += vy_;
    if (y_ >= kGround) y_ = kGround, vy_ = 0;
    // The game's conditions pick the state; the library's rules decide when it may change.
    if (nnh::input::pressed(Button::X)) {
        anim_.request(states_.hurt);
    } else if (nnh::input::pressed(Button::B)) {
        anim_.request(states_.attack);
    } else if (y_ < kGround) {
        anim_.request(vy_ < 0 ? states_.jump : states_.fall);
    } else {
        anim_.request(moving ? states_.run : states_.idle);
    }
    anim_.set_flip(facing_left_);
    anim_.update();
    if (dust_ > 0) --dust_;
    if (hit_ > 0) --hit_;
}

void Hero::draw() const {
    const nnh::Screen bottom = nnh::Screen::Bottom;
    nnh::gfx::fill_rect(bottom, {0, kGround, 320, 40}, nnh::rgb(60, 70, 50));
    nnh::DrawOptions o = anim_.draw_options();
    o.scale = {2, 2};
    nnh::gfx::draw(bottom, texture_, {x_, y_}, o);
    if (dust_ > 0) nnh::gfx::fill_rect(bottom, {x_ - 3, kGround + 2, 6, 3}, nnh::rgb(230, 220, 120));
    if (hit_ > 0) {
        const float hx = facing_left_ ? x_ - 40 : x_ + 10;
        nnh::gfx::draw_rect(bottom, {hx, y_ - 40, 30, 24}, nnh::colors::red);
    }
    char line[96];
    std::snprintf(line, sizeof line, "state %s  clip %s  frame %d  steps %d", library_.state_name(anim_.state()),
                  anim_.clip_name(), anim_.frame(), steps_);
    nnh::gfx::draw_text(bottom, {8, 8}, line);
    nnh::gfx::draw_text(bottom, {8, 22}, "D-Pad run  A jump  B attack  X hurt\nSELECT self-check  START quit",
                        {.color = nnh::rgb(170, 180, 200)});
}
