#include "yard.hpp"

#include "demos.hpp"

#include <nnh/input.hpp>
#include <utility>

using nnh::Entity;
using nnh::Reparent;

namespace yard {
namespace {

constexpr float kTurnStep = nnh::kQuarterTurn / 16; // L/R and the gun's aim: a sixteenth of a quarter per frame
constexpr nnh::Vec2 kBottomCamera{0, 0};
constexpr float kBottomZoom = 0.5f; // the whole yard (640 x 240) on the bottom screen's top half

} // namespace

bool Yard::start() {
    const nnh::ComponentDesc kinds[] = {nnh::node_component(64), nnh::component<Look>(64), nnh::component<Body>(64),
                                        nnh::component<Name>(64)};
    auto world = nnh::World::create({.max_entities = 64, .tags_per_entity = 0}, kinds);
    auto lib = nnh::PrefabLibrary::create({.max_prefabs = 16, .max_entries = 64, .value_bytes = 2048, .max_children = 8});
    auto art = nnh::load_texture("art/yard");
    if (!world || !lib || !art) return false;
    world_ = std::move(world.value());
    lib_ = std::move(lib.value());
    art_ = std::move(art.value());
    tree_ = nnh::Hierarchy(world_);
    tweens_ = nnh::TweenSet({.tweens = 8});
    rig_.set_limits({0, 0, kWidth, kHeight});
    return register_prefabs(lib_, prefabs_);
}

void Yard::restart() {
    tweens_.kill(bob_);
    world_.clear();
    ship_ = lib_.spawn(world_, prefabs_.ship, {96, 120});
    gun_ = tree_.first_child(ship_);
    muzzle_ = tree_.first_child(gun_);
    hand_ = tree_.next_sibling(gun_);
    walker_ = lib_.spawn(world_, prefabs_.walker, {300, 212});
    hat_ = tree_.first_child(walker_);
    cart_ = lib_.spawn(world_, prefabs_.cart, {480, 214});
    rider_ = tree_.first_child(cart_);
    lib_.spawn(world_, prefabs_.squash, {580, 80});
    lib_.spawn(world_, prefabs_.crate, {200, 218});
    lib_.spawn(world_, prefabs_.crate, {240, 218});
    bob_ = tweens_.from_to(&tree_.local(hat_)->position, {0, -11}, {0, -14}, nnh::Frames{30},
                           {.loops = nnh::kForever, .loop_mode = nnh::LoopMode::Yoyo});
    picked_ = {};
    held_ = false;
    walk_dir_ = cart_dir_ = 1;
    for (Shot& s : shots_) s.frames = 0;
    tree_.propagate();
    rig_.snap(tree_.world_transform(rider_)->position());
    NNH_LOG_INFO(example, "yard: restarted nodes %d", tree_.stats().nodes);
}

void Yard::log_at(const char* what, Entity e, const nnh::Vec2& at) const {
    const Name* n = const_cast<nnh::World&>(world_).get<Name>(e);
    NNH_LOG_INFO(example, "%s: %s at %g,%g", what, n != nullptr ? n->text : "?", static_cast<double>(at.x),
                 static_cast<double>(at.y));
}

void Yard::pick(nnh::Vec2 p) {
    const nnh::Storage<Body> bodies = world_.storage<Body>();
    const nnh::collision::BoxView view =
        nnh::collision::box_view(bodies.data(), bodies.extent(), &Body::box, &Body::layers);
    int hits[4];
    const int n = nnh::collision::boxes_at(view, p, hits, 4);
    if (n <= 0) return;
    picked_ = bodies.owner(hits[n > 4 ? 3 : n - 1]); // the last (highest slot): the child over its parent
    held_ = tree_.parent(picked_) == hand_ && hand_;
    const nnh::Vec2 at = tree_.world_transform(picked_)->position();
    NNH_LOG_INFO(example, "picked: %s slot %d gen %d depth %d at %g,%g", world_.get<Name>(picked_)->text,
                 picked_.slot(), static_cast<int>(picked_.bits() >> 16), tree_.depth(picked_),
                 static_cast<double>(at.x), static_cast<double>(at.y));
}

void Yard::read_buttons() {
    namespace input = nnh::input;
    if (input::touch_started()) {
        const nnh::TouchPoint t = input::touch_position();
        pick({kBottomCamera.x + static_cast<float>(t.x) / kBottomZoom, kBottomCamera.y + static_cast<float>(t.y) / kBottomZoom});
    }
    if (input::pressed(nnh::Button::X)) {
        if (!world_.alive(picked_)) {
            restart();
        } else if (held_) {
            tree_.detach(picked_); // keep world: it stays where it was drawn
            held_ = false;
            log_at("dropped", picked_, tree_.world_transform(picked_)->position());
        } else if (world_.alive(hand_) && tree_.attach(picked_, hand_)) {
            held_ = true;
            const Name* n = world_.get<Name>(picked_);
            const nnh::Vec2 at = tree_.world_transform(picked_)->position();
            NNH_LOG_INFO(example, "attached: %s to hand at %g,%g", n->text, static_cast<double>(at.x),
                         static_cast<double>(at.y));
        }
    }
    if (input::pressed(nnh::Button::B) && world_.alive(ship_)) {
        int children = 0;
        tree_.each_descendant(ship_, [&](Entity) { ++children; });
        world_.destroy(ship_);
        NNH_LOG_INFO(example, "destroyed: ship + %d children", children);
    }
    if (input::pressed(nnh::Button::Up) && world_.alive(ship_))
        NNH_LOG_INFO(example, "detached: %d children", tree_.detach_children(ship_));
    if (input::pressed(nnh::Button::Down)) debug_ = !debug_;
    if (input::pressed(nnh::Button::A) && world_.alive(muzzle_)) { // fired from the muzzle, this frame's aim
        const nnh::WorldTransform2D m = tree_.world_now(muzzle_);
        for (Shot& s : shots_) {
            if (s.frames > 0) continue;
            s = {m.position(), nnh::transform_direction(m.map, {3, 0}), 60};
            break;
        }
    }
}

void Yard::move_things() {
    namespace input = nnh::input;
    if (nnh::Transform2D* ship = tree_.local(ship_)) {
        const nnh::Stick pad = input::circle_pad();
        ship->position += nnh::Vec2{pad.x * 2, -pad.y * 2};
        if (input::held(nnh::Button::L)) ship->rotation -= kTurnStep;
        if (input::held(nnh::Button::R)) ship->rotation += kTurnStep;
    }
    if (nnh::Transform2D* gun = tree_.local(gun_)) {
        if (input::held(nnh::Button::Left)) gun->rotation -= kTurnStep;
        if (input::held(nnh::Button::Right)) gun->rotation += kTurnStep;
    }
    if (nnh::Transform2D* walker = tree_.local(walker_)) { // turns at the posts: one scale.x mirrors it and its hat
        walker->position.x += 0.5f * walk_dir_;
        if (walker->position.x >= 380 || walker->position.x <= 260) walk_dir_ = -walk_dir_;
        walker->scale.x = walk_dir_;
    }
    if (nnh::Transform2D* cart = tree_.local(cart_)) {
        cart->position.x += 0.75f * cart_dir_;
        if (cart->position.x >= 600 || cart->position.x <= 440) cart_dir_ = -cart_dir_;
    }
    for (Shot& s : shots_) {
        if (s.frames <= 0) continue;
        s.at += s.v;
        --s.frames;
    }
}

void Yard::update() {
    read_buttons();
    move_things();
    tweens_.update();
    world_.flush();
    tree_.propagate(); // after every move, before the bodies, the camera and drawing
    world_.each<nnh::Node, Body>([](Entity, nnh::Node& n, Body& b) { b.box = nnh::bounds(n.world().map, b.local); });
    const Entity followed = world_.alive(rider_) ? rider_ : ship_;
    if (const nnh::WorldTransform2D* w = tree_.world_transform(followed)) rig_.update(w->position());
}

} // namespace yard
