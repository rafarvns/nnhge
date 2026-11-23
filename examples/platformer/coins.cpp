#include "coins.hpp"

nnh::Entity spawn_coin(nnh::World& world, nnh::Rect bounds, int value) {
    if (world.storage<level::Coin>().count() >= level::kMaxCoins) return {};
    const nnh::Entity e = world.spawn();
    world.add<level::Body>(e, {bounds, layers::kCoin});
    world.add<level::Coin>(e, {value});
    world.add_tag(e, level::kCoinTag);
    return e;
}

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
        if (taken_count < capacity) taken[taken_count] = {e, coin->value};
        ++taken_count;
        world.destroy(e); // its body goes to Body{}: no query finds it again
    }
    return taken_count;
}
