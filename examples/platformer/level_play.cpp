#include "level_play.hpp"

#include "level_camera.hpp"

#include <cstring>
#include <nnh/app.hpp>
#include <nnh/events.hpp>
#include <nnh/gfx.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(platformer);

namespace {

constexpr nnh::Screen kTop = nnh::Screen::Top;

// Draw layers, back to front.
enum DrawLayer : int { kBackdrop = 0, kGround = 2, kThings = 3, kPlayer = 4, kFront = 5 };

bool door_target(const char* to, Demo& out) {
    if (std::strcmp(to, "level1") == 0) {
        out = Demo::Level1;
    } else if (std::strcmp(to, "bonus") == 0) {
        out = Demo::Bonus;
    } else {
        return false;
    }
    return true;
}

} // namespace

LevelPlay::LevelPlay(const Art& art, const nnh::Tilemap& map)
    : art_(art), map_(map), camera_(level_camera_settings(map.bounds())) {
    grid_ = map.collision_grid(layers::kPlayerMask); // the player's view of the level (spec 019)
    grid_.outside = nnh::collision::Cell::Solid; // walls the level in (and a floor under the pits)
    layer_count_ = map.tile_layer_count() < kMaxLayers ? map.tile_layer_count() : kMaxLayers;
    for (int i = 0; i < layer_count_; ++i) {
        if (std::strcmp(map.tile_layer_name(i), "front") == 0)
            draw_layer_[i] = kFront;
        else
            draw_layer_[i] = map.layer_parallax(i).x != 1 ? kBackdrop : kGround;
    }
    read_objects();
}

void LevelPlay::read_objects() {
    for (int i = 0; i < map_.object_count(); ++i) {
        const nnh::MapObject o = map_.object(i);
        Demo to{};
        if (std::strcmp(o.name, "spawn") == 0) {
            spawn_ = {o.bounds.x, o.bounds.y};
        } else if (std::strcmp(o.type, "coin") == 0) {
            if (!coins_.add(o.bounds, o.properties.get_int("value", 1)))
                NNH_LOG_WARN(platformer, "more than %d coins: the rest are left out", CoinBook::kCapacity);
        } else if (std::strcmp(o.type, "turret") == 0) {
            if (!turrets_.add(o.bounds))
                NNH_LOG_WARN(platformer, "more than %d turrets: the rest are left out", TurretBook::kMaxTurrets);
        } else if (std::strcmp(o.type, "door") == 0 && door_count_ < kMaxDoors &&
                   door_target(o.properties.get_string("to"), to)) {
            doors_[door_count_++] = {o.bounds, to};
        }
    }
}

void LevelPlay::restart() {
    player_.spawn(spawn_);
    coins_.restart();
    turrets_.restart();
    in_door_ = false;
    // A fresh rig: no trauma and the shake's noise clock back at 0, so a restart (and the replay that starts with
    // one) plays the same whatever came before.
    camera_ = nnh::CameraRig(level_camera_settings(map_.bounds()));
    look_ahead_ = player_.facing_left() ? -kLookAhead : kLookAhead;
    camera_.set_offset({look_ahead_, 0});
    camera_.snap(player_.box()); // no glide from wherever the camera was
    camera_.apply();
    nnh::events::publish(LevelRestarted{coins_.count()});
}

void LevelPlay::update(const PlayerInput& input, bool doors) {
    const bool was_on_ground = player_.on_ground();
    const float fall_speed = player_.velocity().y;
    player_.update(grid_, input);
    if (!was_on_ground && player_.on_ground()) camera_.add_trauma(landing_trauma(fall_speed)); // a hard landing
    sprite_.update({.on_ground = player_.on_ground(), .velocity = player_.velocity(),
                    .facing_left = player_.facing_left()});
    const nnh::Rect box = player_.box();
    if (box.y + box.h >= map_.bounds().h) {
        NNH_LOG_INFO(platformer, "fell into the water: back to the start");
        restart();
        return;
    }
    take_coins();
    const int hits = turrets_.hits();
    turrets_.update(grid_, box); // after the player: they aim at this frame's position
    if (turrets_.hits() != hits) nnh::events::publish(PlayerHit{turrets_.hits()});    if (doors) enter_doors();
    look_ahead_ = look_ahead_step(look_ahead_, player_.facing_left());
    camera_.set_offset({look_ahead_, 0});
    camera_.update(box); // after moving the player: this frame's position
    camera_.apply();
}

void LevelPlay::take_coins() {
    int taken[8];
    const int count = coins_.collect(player_.box(), layers::kPlayerMask, taken, 8);
    for (int i = 0; i < count && i < 8; ++i)
        nnh::events::publish(CoinCollected{coins_.coin(taken[i]).value});
}

void LevelPlay::enter_doors() {
    bool inside = false;
    for (int i = 0; i < door_count_; ++i) {
        if (!nnh::collision::overlaps(player_.box(), doors_[i].bounds)) continue;
        inside = true;
        if (!in_door_) nnh::events::publish(ShowDemo{doors_[i].to});
    }
    in_door_ = inside;
}

ReplaySample LevelPlay::sample() const {
    return {
        .box = player_.box(),
        .camera = camera_.position(),
        .touching = player_.touching(),
        .coins = coins_.collected(),
        .turrets = turrets_.state()};
}

void LevelPlay::draw() const {
    namespace gfx = nnh::gfx;
    for (int i = 0; i < layer_count_; ++i)
        gfx::draw_tilemap(kTop, map_, {.map_layer = i, .layer = draw_layer_[i]});
    for (int i = 0; i < coins_.count(); ++i) {
        if (coins_.taken(i)) continue;
        const nnh::Rect c = coins_.coin(i).bounds;
        gfx::draw(kTop, art_.tiles, {c.x + c.w / 2 - 8, c.y + c.h / 2 - 8},
                  {.region = tile::region(tile::Coin), .layer = kThings});
    }
    draw_doors();
    draw_turrets();
    sprite_.draw(kTop, art_.player, player_.feet(), kPlayer);
    if (turrets_.flash() > 0 && (turrets_.flash() / 2) % 2 == 0) // hit: the player blinks red
        nnh::gfx::fill_rect(kTop, player_.box(), nnh::rgba(255, 40, 40, 140), {.layer = kFront});
}

void LevelPlay::draw_turrets() const {
    namespace gfx = nnh::gfx;
    const nnh::Rect p = player_.box();
    const nnh::Vec2 target{p.x + p.w * 0.5f, p.y + p.h * 0.5f};
    for (int i = 0; i < turrets_.count(); ++i) {
        const TurretBook::Turret& t = turrets_.turret(i);
        const nnh::Vec2 eye{t.box.x + t.box.w * 0.5f, t.box.y + t.box.h * 0.5f};
        gfx::fill_rect(kTop, {t.box.x + 2, t.box.y + 6, t.box.w - 4, t.box.h - 6}, nnh::rgb(70, 74, 90),
                       {.layer = kThings});
        gfx::fill_rect(kTop, {eye.x - 3, t.box.y + 2, 6, 6}, t.sees ? nnh::rgb(255, 70, 60) : nnh::rgb(120, 120, 130),
                       {.layer = kThings});
        if (t.sees) gfx::draw_line(kTop, eye, target, nnh::rgba(255, 70, 60, 150), {.layer = kThings});
    }
    for (int i = 0; i < TurretBook::kMaxBullets; ++i) {
        const TurretBook::Bullet& b = turrets_.bullet(i);
        if (!b.active) continue;
        // In flight: centred on its position; stuck: just outside the wall, along the wall's normal.
        const nnh::Vec2 c = b.stuck > 0 ? b.position + b.normal * 1.5f : b.position;
        gfx::fill_rect(kTop, {c.x - 1.5f, c.y - 1.5f, 3, 3}, nnh::rgb(255, 230, 90), {.layer = kPlayer});
    }
}

void LevelPlay::draw_doors() const {
    // Two stars that twinkle in turn (frame_count: drawing only, never the game's logic).
    const bool phase = (nnh::frame_count() / 20) % 2 == 0;
    for (int i = 0; i < door_count_; ++i) {
        const nnh::Rect d = doors_[i].bounds;
        nnh::gfx::draw(kTop, art_.tiles, {d.x, phase ? d.y : d.y + d.h - 16},
                       {.region = tile::region(tile::Star), .layer = kThings});
        nnh::gfx::draw(kTop, art_.tiles, {d.x, d.y + d.h / 2 - 8},
                       {.region = tile::region(tile::Lattice), .layer = kThings});
    }
}
