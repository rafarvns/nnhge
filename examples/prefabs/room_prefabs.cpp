// The room's prefabs (spec 021 AC-5): walker, its flying_walker variant, coin, and the roster from the data assets.
// Kept apart from room.cpp, as a game keeps its prefab declarations: registration is load-time code with the hooks'
// lambdas (tween setters), and the room's per-frame code compiles without them.

#include "room.hpp"
#include "roster.hpp"

using nnh::Entity;
using nnh::World;

namespace room {
namespace {

constexpr nnh::Color kGold{255, 200, 40, 255};

/// A coin's value from its map object (`value`, 1 when absent); worth more than 1: gold.
void coin_hook(World& w, Entity e, const nnh::SpawnInfo& info) {
    Coin* coin = w.get<Coin>(e);
    if (coin == nullptr) return;
    if (info.object != nullptr) coin->value = info.object->properties.get_int("value", 1);
    if (coin->value > 1) w.get<Body>(e)->color = kGold;
}

} // namespace

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

} // namespace room
