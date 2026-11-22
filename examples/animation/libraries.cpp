#include "libraries.hpp"

#include <utility>

using nnh::Frames;
using nnh::SpriteClipId;
using nnh::SpriteLibrary;
using nnh::SpriteMode;
using nnh::SpriteSwitch;

SpriteLibrary make_gallery_library() {
    auto made = SpriteLibrary::create({.frames = 8, .clips = kGalleryClips, .clip_frames = 8 * kGalleryClips,
                                       .markers = 2 * kGalleryClips, .pivot = nnh::SpritePivot::Center});
    if (!made) return {};
    SpriteLibrary lib = std::move(made.value());
    (void)lib.add_grid({.frame_width = 16, .frame_height = 16, .count = 8, .image_width = 128, .image_height = 16});
    const nnh::SpriteClipDesc clips[kGalleryClips] = {
        {.name = kGalleryNames[0], .count = 8},
        {.name = kGalleryNames[1], .count = 8, .mode = SpriteMode::Once},
        {.name = kGalleryNames[2], .count = 8, .mode = SpriteMode::Repeat, .repeat = 3},
        {.name = kGalleryNames[3], .count = 8, .mode = SpriteMode::PingPong},
        {.name = kGalleryNames[4], .count = 8, .mode = SpriteMode::PingPong, .repeat = 2,
         .direction = nnh::SpriteDirection::Reverse},
        {.name = kGalleryNames[5], .count = 8, .direction = nnh::SpriteDirection::Reverse},
    };
    for (const nnh::SpriteClipDesc& c : clips) {
        const SpriteClipId id = lib.add_clip(c).value_or({});
        (void)lib.add_marker(id, 0, "tick", 0);
        (void)lib.add_marker(id, 4, "tick", 4);
    }
    return lib;
}

SpriteLibrary make_knight_library() {
    auto made = SpriteLibrary::create({.frames = 13, .clips = 7, .clip_frames = 19, .markers = 3, .states = 7,
                                       .transitions = 5, .pivot = nnh::SpritePivot::BottomCenter});
    if (!made) return {};
    SpriteLibrary lib = std::move(made.value());
    (void)lib.add_grid({.frame_width = 32, .frame_height = 32, .count = 13, .columns = 4});
    const nnh::SpriteClipDesc clips[] = {
        {.name = "idle", .first = 0, .count = 2, .frame_time = Frames{18}, .mode = SpriteMode::PingPong},
        {.name = "run", .first = 2, .count = 4},
        {.name = "run_shoot", .first = 2, .count = 4, .frame_time = Frames{6}},
        {.name = "jump", .first = 6, .count = 1},
        {.name = "fall", .first = 7, .count = 1},
        {.name = "attack", .first = 8, .count = 3, .frame_time = Frames{5}, .mode = SpriteMode::Once},
        {.name = "hurt", .first = 11, .count = 2, .frame_time = Frames{5}, .mode = SpriteMode::Repeat, .repeat = 2},
    };
    for (const nnh::SpriteClipDesc& c : clips) (void)lib.add_clip(c);
    (void)lib.add_marker(lib.find_clip("run"), 0, "step");
    (void)lib.add_marker(lib.find_clip("run"), 2, "step");
    (void)lib.add_marker(lib.find_clip("attack"), 1, "hit", 2);
    if (!add_knight_states(lib)) return {};
    const KnightStates s{lib};
    const nnh::SpriteStateId shoot = lib.add_state("run_shoot", lib.find_clip("run_shoot")).value_or({});
    (void)lib.add_transition({.from = s.run, .to = shoot, .keep_time = true});
    (void)lib.add_transition({.from = shoot, .to = s.run, .keep_time = true});
    return lib;
}

bool add_knight_states(SpriteLibrary& lib) {
    const char* names[] = {"idle", "run", "jump", "fall", "attack", "hurt"};
    for (const char* name : names) {
        if (!lib.add_state(name, lib.find_clip(name))) return false;
    }
    const KnightStates s{lib};
    return lib.set_next_on_finish(s.attack, s.idle).ok() && lib.set_next_on_finish(s.hurt, s.idle).ok() &&
           lib.add_transition({.from = s.attack, .when = SpriteSwitch::Never}).ok() &&
           lib.add_transition({.from = s.attack, .to = s.hurt}).ok() &&
           lib.add_transition({.from = s.hurt, .when = SpriteSwitch::AtEnd}).ok();
}

KnightStates::KnightStates(const SpriteLibrary& lib)
    : idle(lib.find_state("idle")), run(lib.find_state("run")), run_shoot(lib.find_state("run_shoot")),
      jump(lib.find_state("jump")), fall(lib.find_state("fall")), attack(lib.find_state("attack")),
      hurt(lib.find_state("hurt")) {}
