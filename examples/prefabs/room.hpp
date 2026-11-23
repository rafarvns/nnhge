#pragma once
// The prefabs example's room (spec 021 AC-5): a world (bound to the showcase scene, then its tween set) and a prefab
// library (WhenFull::Miss: a touch past a budget is a quiet miss) holding `walker`, its variant `flying_walker` (a Bob
// and the tag "flying"; its hook starts a bobbing tween), `coin` (its hook reads the map object's `value` and turns a
// coin worth more than 1 gold) and one prefab per enemy data asset (roster.hpp). A room made in Tiled
// (assets/rooms/room.tmj) names them by type or by its `prefab` property; its door is the game's own (skipped).
// Log lines (examples/prefabs/tests/check.nnhs expects them): "map: spawned N skipped N unknown N misses N",
// "spawned: <name> slot n gen g at x,y tags a,b", "missed: <name> (misses N)", "selected: <name> kinds ... tags ...".

#include "components.hpp"
#include "tags.hpp"

#include <nnh/entities.hpp>
#include <nnh/prefabs.hpp>
#include <nnh/scene.hpp>
#include <nnh/tilemap.hpp>
#include <nnh/tween.hpp>

namespace room {

inline constexpr int kMaxPrefabs = 16;

class Room {
  public:
    /// Binds the world, then the tween set, to `scene` (its owner).
    explicit Room(nnh::Scene& scene);
    ~Room();
    Room(const Room&) = delete;
    Room& operator=(const Room&) = delete;

    /// Loads the room's map and registers the prefabs (the roster from data assets). False on an error (logged).
    bool load();
    /// clear() and the map's objects spawned again (the first load's slots); logs the map line.
    nnh::MapSpawnSummary restart();
    /// One frame: walkers walk and turn at the walls.
    void update();
    /// The selected prefab at world point `at` (its placement member); logs "spawned: ..." or "missed: ...".
    nnh::Entity spawn_selected(nnh::Vec2 at);
    /// A gold coin at the room's centre: the coin prefab with a component override, Coin{5}.
    nnh::Entity spawn_gold();
    /// Moves the selection by `delta` (L/R) and logs "selected: <name> kinds <list> tags <list>".
    void select(int delta);
    /// "kinds Body,Walker tags enemy" for the selected prefab (the HUD's debug line).
    void describe_selected(char* out, int size) const;

    nnh::World& world() { return world_; }
    const nnh::Tilemap& map() const { return map_; }
    const nnh::PrefabLibrary& prefabs() const { return prefabs_; }
    nnh::Prefab selected() const { return list_[selected_]; }
    const nnh::MapSpawnSummary& last_map() const { return last_map_; }
    int roster_count() const { return roster_count_; }
    nnh::Prefab roster(int i) const { return list_[kRosterFirst + i]; }

  private:
    static constexpr int kRosterFirst = 3; ///< walker, flying_walker, coin, then the roster
    bool bind_world(nnh::Scene& scene);
    bool register_prefabs();
    void log_spawn(nnh::Entity e, nnh::Prefab p, nnh::Vec2 at);

    nnh::World world_;     // bound first (by bound_): its Bob hook kills tweens in the set
    bool bound_;           // initialised between world_ and tweens_
    nnh::TweenSet tweens_; // bound second
    nnh::PrefabLibrary prefabs_;
    nnh::Tilemap map_;
    nnh::Prefab list_[kMaxPrefabs] = {};
    int count_ = 0;
    int roster_count_ = 0;
    int selected_ = 0;
    nnh::MapSpawnSummary last_map_{};
};

/// "Body", "Walker", ... for a kind of the room ("?" for another).
const char* kind_name(nnh::KindId id);

} // namespace room
