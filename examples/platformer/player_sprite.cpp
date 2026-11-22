#include "player_sprite.hpp"

#include <utility>

namespace {

nnh::SpriteLibrary make_library() {
    auto made = nnh::SpriteLibrary::create({.frames = 4, .clips = 4, .clip_frames = 5, .markers = 2, .states = 4,
                                            .pivot = nnh::SpritePivot::BottomCenter});
    if (!made) return {};
    nnh::SpriteLibrary lib = std::move(made.value());
    (void)lib.add_grid({.frame_width = 16, .frame_height = 16, .count = 4, .image_width = 64, .image_height = 16});
    const char* names[] = {"idle", "run", "jump", "fall"};
    const int first[] = {0, 1, 3, 3};
    const int count[] = {1, 2, 1, 1};
    for (int i = 0; i < 4; ++i) {
        const nnh::SpriteClipId clip =
            lib.add_clip({.name = names[i], .first = first[i], .count = count[i], .frame_time = nnh::Frames{8}})
                .value_or({});
        (void)lib.add_state(names[i], clip);
    }
    (void)lib.add_marker(lib.find_clip("run"), 0, "step");
    (void)lib.add_marker(lib.find_clip("run"), 1, "step");
    return lib;
}

} // namespace

PlayerSprite::PlayerSprite() : library_(make_library()) {
    idle_ = library_.find_state("idle");
    run_ = library_.find_state("run");
    jump_ = library_.find_state("jump");
    fall_ = library_.find_state("fall");
    if (library_) anim_ = nnh::SpriteAnimator{library_, idle_};
}

void PlayerSprite::update(const PlayerLook& look) {
    if (!library_) return;
    const bool moving = look.velocity.x != 0;
    anim_.request(look.on_ground ? (moving ? run_ : idle_) : (look.velocity.y < 0 ? jump_ : fall_));
    anim_.set_flip(look.facing_left);
    stepped_ = false;
    for (const nnh::SpriteNotice& n : anim_.update()) {
        if (n.kind == nnh::SpriteNoticeKind::Marker && n.marker == nnh::sprite_name("step")) stepped_ = true;
    }
    if (dust_ > 0) --dust_;
    if (stepped_) dust_ = 6;
}

void PlayerSprite::draw(nnh::Screen screen, const nnh::Texture& sheet, nnh::Vec2 feet, int layer) const {
    nnh::DrawOptions o = anim_.draw_options();
    o.layer = layer;
    nnh::gfx::draw(screen, sheet, feet, o);
    if (dust_ > 0) {
        const float spread = static_cast<float>(7 - dust_); // the puff opens as it fades
        const nnh::Color c = nnh::rgba(230, 220, 190, static_cast<std::uint8_t>(40 * dust_));
        nnh::gfx::fill_rect(screen, {feet.x - 4 - spread, feet.y - 2, 2, 2}, c, {.layer = layer});
        nnh::gfx::fill_rect(screen, {feet.x + 2 + spread, feet.y - 2, 2, 2}, c, {.layer = layer});
    }
}
