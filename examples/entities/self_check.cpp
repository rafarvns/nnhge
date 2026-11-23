#include "self_check.hpp"

#include <cstdio>

using nnh::Entity;
using nnh::Tag;
using nnh::World;

namespace {

constexpr std::uint32_t kSeed = 0x0E57C0DEu;
constexpr Tag kTags[6] = {nnh::tag("enemy"), nnh::tag("coin"), nnh::tag("flying"),
                          nnh::tag("fx"),    nnh::tag("boss"), nnh::tag("pickup")};
static_assert(nnh::tags_distinct(kTags[0], kTags[1], kTags[2], kTags[3], kTags[4], kTags[5]));

} // namespace

bool EntitiesSelfCheck::start() {
    const nnh::ComponentDesc kinds[] = {
        nnh::component<A>(kEntities),
        nnh::component<B>(kEntities, [this](World&, Entity, B& b) { on_b_removed(b.v); }),
        nnh::component<C>(kEntities),
    };
    auto world = World::create({.max_entities = kEntities, .tags_per_entity = 4}, kinds);
    auto pool = nnh::Pool<Item>::create(kPoolSize);
    if (!world || !pool) return false;
    world_ = static_cast<World&&>(world.value());
    pool_ = static_cast<nnh::Pool<Item>&&>(pool.value());
    return true;
}

std::uint32_t EntitiesSelfCheck::hash(std::uint32_t salt) const {
    const std::uint32_t words[3] = {kSeed, static_cast<std::uint32_t>(step_), salt};
    char bytes[12];
    for (int i = 0; i < 12; ++i) bytes[i] = static_cast<char>((words[i / 4] >> (8 * (i % 4))) & 0xFFu);
    return nnh::detail::fnv1a32(bytes, sizeof bytes);
}

void EntitiesSelfCheck::mix(std::uint32_t word) {
    for (int i = 0; i < 4; ++i) {
        sum_ ^= (word >> (8 * i)) & 0xFFu;
        sum_ *= 16777619u;
    }
}

void EntitiesSelfCheck::on_b_removed(int value) {
    if (value % 5 == 0 && world_.destroy(handles_[(value / 5) % kEntities])) ++stats_.hook_destroys;
    if (value % 17 == 0 && !world_.clearing() && world_.count() < kLiveLimit) {
        const Entity n = world_.spawn();
        if (!n) return;
        handles_[n.slot()] = n;
        world_.add<A>(n, {value, 1, 2, 3});
        ++stats_.hook_spawns;
        mix(n.bits());
    }
}

Entity EntitiesSelfCheck::pick(std::uint32_t h) const { return handles_[h % kEntities]; }

void EntitiesSelfCheck::spawn_one(std::uint32_t h) {
    if (world_.count() >= kLiveLimit) return;
    const Entity e = world_.spawn();
    mix(e.bits());
    if (!e) return;
    ++stats_.spawns;
    handles_[e.slot()] = e;
    const int v = static_cast<int>((h >> 8) & 0x3FF);
    world_.add<A>(e, {v, v + 1, v + 2, v + 3});
    if ((h >> 18) & 1u) world_.add<B>(e, {v + 7, 0});
    if ((h >> 19) % 3 == 0) world_.add<C>(e, {v});
    for (int t = 0; t < static_cast<int>((h >> 21) % 3); ++t) world_.add_tag(e, kTags[(h >> (23 + 2 * t)) % 6]);
}

void EntitiesSelfCheck::world_op(int op, std::uint32_t h) {
    const Entity e = pick(h >> 5);
    const int v = static_cast<int>((h >> 16) & 0x3FF);
    switch (op) {
    case 6: case 7: case 8: case 9:
        if (world_.destroy(e)) ++stats_.destroys;
        break;
    case 10:
        if (world_.alive(e) && !world_.has<B>(e)) mix(world_.add<B>(e, {v, 0}) != nullptr ? 1u : 0u);
        break;
    case 11:
        if (world_.alive(e) && !world_.has<C>(e)) mix(world_.add<C>(e, {v}) != nullptr ? 1u : 0u);
        break;
    case 12:
        mix(world_.remove<B>(e) ? 1u : 0u);
        break;
    default:
        mix(world_.remove<C>(e) ? 1u : 0u);
        break;
    }
}

void EntitiesSelfCheck::iterate(int op, std::uint32_t h) {
    const int m = 7 + static_cast<int>((h >> 8) % 7);
    if (op == 14) {
        world_.each<A>([&](Entity e, A& a) {
            mix(e.bits());
            ++stats_.visits;
            a.w += a.v;
            if (a.v % m == 0) world_.destroy(e);
        });
    } else if (op == 15) {
        world_.each<B, A>([&](Entity e, B& b, A& a) {
            mix(e.bits() ^ static_cast<std::uint32_t>(a.w));
            ++stats_.visits;
            if (b.v % m == 0) world_.remove<A>(e);
            if (b.v % 11 == 3 && world_.count() < kLiveLimit) {
                const Entity n = world_.spawn();
                if (n) handles_[n.slot()] = n;
                world_.add<A>(n, {b.v, 0, 0, 0});
            }
        });
    } else {
        world_.each<A, B, C>([&](Entity e, A& a, B& b, C& c) {
            mix(e.bits());
            mix(static_cast<std::uint32_t>(a.v + b.v + c.v));
            ++stats_.visits;
        });
    }
}

void EntitiesSelfCheck::tag_op(int op, std::uint32_t h) {
    const Tag t = kTags[(h >> 8) % 6];
    const Entity e = pick(h >> 12);
    if (op == 17) {
        // A full entity would assert: add only where there is room (or the tag is there already).
        int tags = 0;
        for (Tag x : kTags) tags += world_.has_tag(e, x) ? 1 : 0;
        if (tags < 4 || world_.has_tag(e, t)) mix(world_.add_tag(e, t) ? 1u : 0u);
    } else if (op == 18) {
        mix(world_.remove_tag(e, t) ? 1u : 0u);
    } else if (op == 19) {
        Entity out[8];
        const int total = world_.find(t, out, 8);
        ++stats_.finds;
        mix(static_cast<std::uint32_t>(total));
        for (int i = 0; i < total && i < 8; ++i) mix(out[i].bits());
        mix(world_.first(t).bits());
    } else {
        world_.each_tagged(t, [&](Entity x) {
            mix(x.bits());
            ++stats_.visits;
            if ((x.bits() >> 16) % 4 == 1) world_.destroy(x);
        });
    }
}

void EntitiesSelfCheck::pool_op(int op, std::uint32_t h) {
    if (op <= 23) {
        auto [handle, item] = pool_.acquire({static_cast<std::int32_t>(h >> 8)});
        mix(handle.bits());
        if (!handle) {
            ++stats_.pool_misses;
            return;
        }
        ++stats_.acquires;
        items_[handle.slot()] = handle;
        (void)item;
    } else if (op <= 25) {
        mix(pool_.release(items_[(h >> 8) % kPoolSize]) ? 1u : 0u);
    } else if (op == 26) {
        for (auto [handle, item] : pool_.items()) {
            mix(handle.bits() ^ static_cast<std::uint32_t>(item.v));
            if (item.v % 3 == 0) pool_.release(handle);
        }
    } else {
        pool_.flush();
    }
}

bool EntitiesSelfCheck::step() {
    if (done()) return false;
    const std::uint32_t h = hash(0);
    const int op = static_cast<int>(h % 32);
    if (op < 6) {
        spawn_one(h);
    } else if (op < 14) {
        world_op(op, h);
    } else if (op < 17) {
        iterate(op, h);
    } else if (op < 21) {
        tag_op(op, h);
    } else if (op < 28) {
        pool_op(op, h);
    } else if (op == 28 && (h >> 8) % 64 == 0) {
        world_.clear();
        ++stats_.clears;
    } else {
        world_.flush();
    }
    if (step_ % 50 == 49) { // keeps room for the next operations (a full world asserts)
        world_.flush();
        pool_.flush();
    }
    const nnh::WorldStats s = world_.stats();
    if (s.entities > stats_.max_live) stats_.max_live = s.entities;
    ++step_;
    if (step_ % kCheckpointEvery != 0) return false;
    mix(static_cast<std::uint32_t>(s.entities));
    mix(static_cast<std::uint32_t>(s.peak));
    mix(static_cast<std::uint32_t>(pool_.count()));
    mix(static_cast<std::uint32_t>(pool_.stats().misses));
    return true;
}

void format_entities_checkpoint(char* out, std::size_t size, int operations, std::uint32_t sum) {
    std::snprintf(out, size, "entities: %d sum %08X", operations, static_cast<unsigned>(sum));
}

void format_entities_verdict(char* out, std::size_t size, std::uint32_t sum) {
    if (sum == kEntitiesCheckGolden) {
        std::snprintf(out, size, "entities: PASS %08X", static_cast<unsigned>(sum));
    } else {
        std::snprintf(out, size, "entities: FAIL sum %08X expected %08X", static_cast<unsigned>(sum),
                      static_cast<unsigned>(kEntitiesCheckGolden));
    }
}
