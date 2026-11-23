#include "room.hpp"

#include "roster.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

using nnh::Entity;
using nnh::World;

namespace room {
namespace {

constexpr nnh::Color kGold{255, 200, 40, 255};
constexpr nnh::Tag kTagOrder[] = {kEnemy, kFlying, kCoin, kRoster};

World create_world(nnh::TweenSet* tweens) {
    const nnh::ComponentDesc kinds[] = {
        nnh::component<Body>(kMaxEntities),
        nnh::component<Walker>(kMaxWalkers),
        // A flying walker that goes takes its bobbing tween with it.
        nnh::component<Bob>(kMaxWalkers, [tweens](World&, Entity, Bob& b) { tweens->kill(b.tween); }),
        nnh::component<Coin>(kMaxCoins),
        nnh::component<Stats>(kMaxWalkers),
    };
    auto made = World::create({.max_entities = kMaxEntities, .tags_per_entity = 2}, kinds);
    NNH_VERIFY(made.ok(), "the room's world: out of memory");
    return static_cast<World&&>(made.value());
}

/// "enemy,flying" (the entity's tags in a fixed order), or "-".
void tag_list(World& w, Entity e, char* out, int size) {
    int at = 0;
    out[0] = '\0';
    for (nnh::Tag t : kTagOrder)
        if (w.has_tag(e, t)) at += std::snprintf(out + at, size - at, "%s%s", at == 0 ? "" : ",", tag_name(t));
    if (at == 0) std::snprintf(out, size, "-");
}

/// A coin's value from its map object (`value`, 1 when absent); worth more than 1: gold.
void coin_hook(World& w, Entity e, const nnh::SpawnInfo& info) {
    Coin* coin = w.get<Coin>(e);
    if (coin == nullptr) return;
    if (info.object != nullptr) coin->value = info.object->properties.get_int("value", 1);
    if (coin->value > 1) w.get<Body>(e)->color = kGold;
}

} // namespace

const char* kind_name(nnh::KindId id) {
    if (id == nnh::kind_id<Body>()) return "Body";
    if (id == nnh::kind_id<Walker>()) return "Walker";
    if (id == nnh::kind_id<Bob>()) return "Bob";
    if (id == nnh::kind_id<Coin>()) return "Coin";
    if (id == nnh::kind_id<Stats>()) return "Stats";
    return "?";
}

Room::Room(nnh::Scene& scene)
    : world_(create_world(&tweens_)), bound_(bind_world(scene)), tweens_(scene, {.tweens = 32}) {}

// Members go in reverse order, the tween set before the world: empty the world first (Bob's hook kills tweens).
Room::~Room() { world_.clear(); }

bool Room::bind_world(nnh::Scene& scene) {
    world_.bind(scene);
    return true;
}

bool Room::load() {
    if (map_.valid()) return true;
    auto map = nnh::load_tilemap("rooms/room");
    if (!map) {
        NNH_LOG_ERROR(example, "rooms/room: %s", map.error().message);
        return false;
    }
    map_ = static_cast<nnh::Tilemap&&>(map.value());
    return register_prefabs();
}

bool Room::register_prefabs() {
    auto made = nnh::PrefabLibrary::create(
        {.max_prefabs = kMaxPrefabs, .max_entries = 32, .value_bytes = 512, .when_full = nnh::WhenFull::Miss});
    if (!made) return false;
    prefabs_ = static_cast<nnh::PrefabLibrary&&>(made.value());
    const nnh::Placement box = nnh::place(&Body::box);
    nnh::TweenSet* tweens = &tweens_;
    auto walker = prefabs_.add("walker", {.tags = {kEnemy}, .place = box}, Body{{0, 0, 12, 12}, {220, 90, 70, 255}},
                               Walker{0.5f});
    if (!walker) return false;
    // The variant: the walker's body and walk, plus a Bob and "flying"; its hook starts the bobbing (a setter that
    // captures the world and the handle writes only while the flyer lives).
    auto flyer = prefabs_.add("flying_walker",
                              {.base = walker.value(),
                               .tags = {kFlying},
                               .on_spawn =
                                   [tweens](World& w, Entity e, const nnh::SpawnInfo&) {
                                       World* world = &w;
                                       Bob* bob = w.get<Bob>(e);
                                       if (bob == nullptr) return;
                                       bob->tween = tweens->from_to(
                                           nnh::Setter<float>{[world, e](float v) {
                                               if (Bob* b = world->get<Bob>(e)) b->offset = v;
                                           }},
                                           0.0f, 8.0f, nnh::Frames{36},
                                           {.ease = nnh::Ease::SineInOut, .loops = nnh::kForever,
                                            .loop_mode = nnh::LoopMode::Yoyo});
                                   }},
                              Bob{}, Walker{0.75f});
    auto coin = prefabs_.add("coin", {.tags = {kCoin}, .place = box, .on_spawn = coin_hook},
                             Body{{0, 0, 8, 8}, {250, 240, 120, 255}}, Coin{1});
    if (!flyer || !coin) return false;
    list_[0] = walker.value();
    list_[1] = flyer.value();
    list_[2] = coin.value();
    roster_count_ = register_roster(prefabs_, list_ + kRosterFirst, kMaxPrefabs - kRosterFirst);
    count_ = kRosterFirst + roster_count_;
    return true;
}

nnh::MapSpawnSummary Room::restart() {
    world_.clear();
    last_map_ = prefabs_.spawn_map_objects(world_, map_);
    NNH_LOG_INFO(example, "map: spawned %d skipped %d unknown %d misses %d", last_map_.spawned, last_map_.skipped,
                 last_map_.unknown, last_map_.misses);
    return last_map_;
}

void Room::update() {
    world_.each<Walker, Body>([](Entity, Walker& w, Body& b) {
        b.box.x += w.vx;
        if (b.box.x < kInside.x || b.box.x + b.box.w > kInside.x + kInside.w) {
            w.vx = -w.vx; // turn at the wall
            b.box.x += 2 * w.vx;
        }
    });
}

void Room::log_spawn(Entity e, nnh::Prefab p, nnh::Vec2 at) {
    if (!e) {
        NNH_LOG_INFO(example, "missed: %s (misses %d)", prefabs_.name(p), prefabs_.stats().misses);
        return;
    }
    char tags[48];
    tag_list(world_, e, tags, sizeof tags);
    NNH_LOG_INFO(example, "spawned: %s slot %d gen %d at %d,%d tags %s", prefabs_.name(p), e.slot(), e.generation(),
                 static_cast<int>(at.x), static_cast<int>(at.y), tags);
}

Entity Room::spawn_selected(nnh::Vec2 at) {
    const nnh::Prefab p = list_[selected_];
    const Entity e = prefabs_.spawn(world_, p, at);
    log_spawn(e, p, at);
    return e;
}

Entity Room::spawn_gold() {
    const nnh::Vec2 at{196, 116};
    const Entity e = prefabs_.spawn_with(world_, list_[2], at, Coin{5}); // a component override for this copy only
    log_spawn(e, list_[2], at);
    return e;
}

void Room::describe_selected(char* out, int size) const {
    const nnh::Prefab p = list_[selected_];
    nnh::KindId kinds[8];
    nnh::Tag tags[8];
    const int nk = prefabs_.kinds(p, kinds, 8);
    const int nt = prefabs_.tags(p, tags, 8);
    int at = std::snprintf(out, size, "kinds ");
    for (int i = 0; i < nk && i < 8 && at < size; ++i)
        at += std::snprintf(out + at, size - at, "%s%s", i == 0 ? "" : ",", kind_name(kinds[i]));
    if (at < size) at += std::snprintf(out + at, size - at, " tags ");
    for (int i = 0; i < nt && i < 8 && at < size; ++i)
        at += std::snprintf(out + at, size - at, "%s%s", i == 0 ? "" : ",", tag_name(tags[i]));
}

void Room::select(int delta) {
    if (count_ == 0) return;
    selected_ = (selected_ + delta + count_) % count_;
    char line[96];
    describe_selected(line, sizeof line);
    NNH_LOG_INFO(example, "selected: %s %s", prefabs_.name(list_[selected_]), line);
}

} // namespace room
