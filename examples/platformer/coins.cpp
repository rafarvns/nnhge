#include "coins.hpp"

int collect_coins(nnh::World& world, nnh::Rect box, nnh::collision::Layers mask, TakenCoin* taken, int capacity) {
    namespace collision = nnh::collision;
    const nnh::Storage<level::Body> bodies = world.storage<level::Body>();
    int found[level::kMaxEntities];
    const collision::BoxView view =
        collision::box_view(bodies.data(), bodies.extent(), &level::Body::box, &level::Body::layers);
    const int count = collision::boxes_overlapping(view, box, found, level::kMaxEntities, {.mask = mask});
    int taken_count = 0;
    for (int i = 0; i < count; ++i) {
        const nnh::Entity e = bodies.owner(found[i]);
        const level::Coin* coin = world.get<level::Coin>(e);
        if (coin == nullptr) continue; // something else on the coin layer: not a coin
        const nnh::Rect b = bodies.data()[found[i]].box;
        if (taken_count < capacity) taken[taken_count] = {e, coin->value, {b.x + b.w * 0.5f, b.y + b.h * 0.5f}};
        ++taken_count;
        world.destroy(e); // its body goes to Body{}: no query finds it again
    }
    return taken_count;
}
