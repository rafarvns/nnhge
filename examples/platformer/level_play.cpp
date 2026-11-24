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

nnh::World make_world(nnh::TweenSet* tweens) {
    auto world = level::create_world(tweens);
    NNH_VERIFY(world.ok(), "the level's world: out of memory");
    return static_cast<nnh::World&&>(world.value());
}

level::LevelPrefabs make_prefabs(nnh::TweenSet* tweens) {
    auto prefabs = level::create_level_prefabs(tweens);
    NNH_VERIFY(prefabs.ok(), "the level's prefabs: out of memory");
    return static_cast<level::LevelPrefabs&&>(prefabs.value());
}

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

LevelPlay::LevelPlay(const Art& art, const nnh::Tilemap& map, nnh::Scene& scene)
    : art_(art), map_(map), camera_(level_camera_settings(map.bounds())), world_(make_world(&tweens_)),
      world_bound_(bind_world(scene)), tweens_(scene, {.tweens = 128}), prefabs_(make_prefabs(&tweens_)) {
    turrets_.bullets().bind(scene);
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

// D30 runs attachments in attach order: the world's exit clear (its Spark hooks kill their fades) comes before the
// tween set's own clear.
bool LevelPlay::bind_world(nnh::Scene& scene) {
    world_.bind(scene);
    return true;
}

void LevelPlay::read_objects() {
    for (int i = 0; i < map_.object_count(); ++i) {
        const nnh::MapObject o = map_.object(i);
        Demo to{};
        if (std::strcmp(o.name, "spawn") == 0) {
            spawn_ = {o.bounds.x, o.bounds.y};
        } else if (std::strcmp(o.type, "door") == 0 && door_count_ < kMaxDoors &&
                   door_target(o.properties.get_string("to"), to)) {
            doors_[door_count_++] = {o.bounds, to};
        }
    }
}

/// The level's things in the order of the first load: the player, then the map's coin and turret objects in object
/// order, by prefab (doors and the spawn point are skipped types: read_objects() reads them).
void LevelPlay::spawn_things() {
    world_.clear();
    player_entity_ = world_.spawn();
    world_.add<level::Body>(player_entity_, {player_.box(), layers::kPlayer});
    world_.add_tag(player_entity_, level::kPlayerTag);
    prefabs_.library.spawn_map_objects(world_, map_);
    coin_count_ = world_.storage<level::Coin>().count();
}

void LevelPlay::restart() {
    player_.spawn(spawn_);
    spawn_things();
    collected_ = 0;
    turrets_.restart();
    effects_.clear();
    in_door_ = false;
    // A fresh rig: no trauma and the shake's noise clock back at 0, so a restart (and the replay that starts with
    // one) plays the same whatever came before.
    camera_ = nnh::CameraRig(level_camera_settings(map_.bounds()));
    look_ahead_ = player_.facing_left() ? -kLookAhead : kLookAhead;
    camera_.set_offset({look_ahead_, 0});
    camera_.snap(player_.box()); // no glide from wherever the camera was
    camera_.apply();
    nnh::events::publish(LevelRestarted{coin_count_});
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
    world_.get<level::Body>(player_entity_)->box = box; // the player's body: the one per-frame write
    sparkles_.update();
    take_coins();
    const int hits = turrets_.hits();
    turrets_.update(grid_, player_entity_); // after the player: they aim at this frame's position
    tree_.propagate(); // spec 022: the barrels' world transforms, after the turrets aimed, before the camera and drawing
    if (turrets_.hits() != hits) nnh::events::publish(PlayerHit{turrets_.hits()});
    if (doors) enter_doors();
    look_ahead_ = look_ahead_step(look_ahead_, player_.facing_left());
    camera_.set_offset({look_ahead_, 0});
    camera_.update(box); // after moving the player: this frame's position
    camera_.apply();
    effects_.update({player_, turrets_, world_, tree_}); // spec 024: reads this frame, writes nothing of it
}

void LevelPlay::take_coins() {
    TakenCoin taken[8];
    const int count = collect_coins(world_, player_.box(), layers::kPlayerMask, taken, 8);
    collected_ += count;
    for (int i = 0; i < count && i < 8; ++i) {
        nnh::events::publish(CoinCollected{taken[i].value});
        sparkles_.burst(taken[i].centre);
    }
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

ReplaySample LevelPlay::sample() {
    return {
        .box = player_.box(),
        .camera = camera_.position(),
        .touching = player_.touching(),
        .coins = collected_,
        .turrets = turrets_.state(),
        .entities = world_.count(),
        .handles = handle_hash()};
}

// The slots, not the generations: a restart clears the world but never resets generations (stale handles from
// before it must stay stale), so they carry whatever was played before the replay; the live slots of each frame are
// the same whatever came before, and they change with every spawn and destroy.
std::uint32_t LevelPlay::handle_hash() {
    std::uint32_t sum = kChecksumStart;
    for (const nnh::Entity e : world_.entities()) sum = checksum_word(sum, static_cast<std::uint32_t>(e.slot()));
    return sum;
}

void LevelPlay::draw() {
    namespace gfx = nnh::gfx;
    for (int i = 0; i < layer_count_; ++i)
        gfx::draw_tilemap(kTop, map_, {.map_layer = i, .layer = draw_layer_[i]});
    world_.each<level::Coin, level::Body>([&](nnh::Entity, level::Coin&, level::Body& body) {
        const nnh::Rect c = body.box;
        gfx::draw(kTop, art_.tiles, {c.x + c.w / 2 - 8, c.y + c.h / 2 - 8},
                  {.region = tile::region(tile::Coin), .layer = kThings});
    });
    draw_doors();
    draw_turrets();
    draw_sparkles();
    effects_.draw(kPlayer);
    sprite_.draw(kTop, art_.player, player_.feet(), kPlayer);
    if (turrets_.flash() > 0 && (turrets_.flash() / 2) % 2 == 0) // hit: the player blinks red
        nnh::gfx::fill_rect(kTop, player_.box(), nnh::rgba(255, 40, 40, 140), {.layer = kFront});
}

void LevelPlay::draw_turrets() {
    namespace gfx = nnh::gfx;
    const nnh::Rect p = player_.box();
    const nnh::Vec2 target{p.x + p.w * 0.5f, p.y + p.h * 0.5f};
    // Spec 022: the base and its barrel (a child node) drawn from their world transforms; the barrel turns as it aims.
    world_.each<level::Turret, nnh::Node>([&](nnh::Entity e, level::Turret& t, nnh::Node& node) {
        gfx::draw(kTop, art_.turret, node.world(), {.region = {0, 0, 16, 16}, .pivot = {8, 8}, .layer = kThings});
        if (const nnh::Node* barrel = world_.get<nnh::Node>(tree_.first_child(e)))
            gfx::draw(kTop, art_.turret, barrel->world(),
                      {.region = {16, 0, 12, 4}, .pivot = {0, 2}, .tint = t.sees ? nnh::rgb(255, 150, 140) : nnh::colors::white,
                       .layer = kThings});
        if (t.sees) gfx::draw_line(kTop, node.world().position(), target, nnh::rgba(255, 70, 60, 150), {.layer = kThings});
    });
    for (auto [h, b] : turrets_.bullets().items()) {
        // In flight: centred on its position; stuck: just outside the wall, along the wall's normal.
        const nnh::Vec2 c = b.stuck > 0 ? b.position + b.normal * 1.5f : b.position;
        gfx::fill_rect(kTop, {c.x - 1.5f, c.y - 1.5f, 3, 3}, nnh::rgb(255, 230, 90), {.layer = kPlayer});
    }
}

void LevelPlay::draw_sparkles() {
    world_.each<level::Spark>([](nnh::Entity, level::Spark& s) {
        const auto a = static_cast<std::uint8_t>(s.alpha * 255.0f);
        nnh::gfx::fill_rect(kTop, {s.position.x - 1, s.position.y - 1, 3, 3}, nnh::rgba(255, 244, 160, a),
                            {.layer = kPlayer});
    });
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
