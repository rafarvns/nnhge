#pragma once
// The hierarchy showcase's yard (spec 022 AC-5): a 640x240 world of hierarchies spawned from prefabs (yard_prefabs.cpp)
// — a ship with a gun (and its muzzle) and a hand, a walker with a bobbing hat, a cart with a rider, a squashed block
// with a turned child (shear), two crates — and the frame order every hierarchy game follows:
//   input → moves and tweens → propagate() → bodies from bounds → camera rig → draw (yard_draw.cpp).
// Log lines (the console script expects them): "yard: restarted nodes <n>", "picked: <name> slot <s> gen <g> depth <d>
// at <x>,<y>", "attached: <name> to hand at <x>,<y>", "dropped: <name> at <x>,<y>", "detached: <n> children",
// "destroyed: ship + <n> children". Positions are binary fractions, so the prints are exact.

#include <cstdint>

#include <nnh/camera.hpp>
#include <nnh/collision.hpp>
#include <nnh/gfx.hpp>
#include <nnh/hierarchy.hpp>
#include <nnh/prefabs.hpp>
#include <nnh/texture.hpp>
#include <nnh/tween.hpp>

namespace yard {

/// How a node is drawn: a region of yard.png and its pivot (centred: mirrors stay in place).
struct Look {
    nnh::Region region{};
    nnh::Vec2 pivot{};
};
/// A pickable node's box in its own space and, after the pass, in the world (bounds of the local box).
struct Body {
    nnh::Rect local{};
    nnh::Rect box{};
    nnh::collision::Layers layers = nnh::collision::kNoLayers;
};
struct Name {
    const char* text = "";
};

struct Prefabs {
    nnh::Prefab ship{}, walker{}, cart{}, squash{}, crate{};
};
/// Registers the yard's prefabs (false: a registration failed).
bool register_prefabs(nnh::PrefabLibrary& lib, Prefabs& out);

class Yard {
public:
    static constexpr float kWidth = 640, kHeight = 240;
    static constexpr int kShots = 16;

    bool start(); ///< world, library, tweens, art (false: out of memory or art missing)
    void restart();
    void update();
    void draw() const;

private:
    struct Shot {
        nnh::Vec2 at{}, v{};
        int frames = 0;
    };
    void read_buttons();
    void pick(nnh::Vec2 world_point);
    void move_things();
    void log_at(const char* what, nnh::Entity e, const nnh::Vec2& at) const;
    void draw_nodes(nnh::Screen screen) const;
    void draw_hud() const;

    nnh::World world_;
    nnh::Hierarchy tree_;
    nnh::TweenSet tweens_;
    nnh::PrefabLibrary lib_;
    Prefabs prefabs_{};
    nnh::Texture art_;
    nnh::CameraRig rig_;
    nnh::Entity ship_{}, gun_{}, muzzle_{}, hand_{}, walker_{}, hat_{}, cart_{}, rider_{};
    nnh::Entity picked_{};
    nnh::TweenHandle bob_{};
    Shot shots_[kShots] = {};
    float walk_dir_ = 1, cart_dir_ = 1;
    bool held_ = false, debug_ = false;
};

} // namespace yard
