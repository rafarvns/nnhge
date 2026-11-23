// The world-versus-model driver (spec 020 T3.4, T4.3, T4.4): see model_driver.hpp.

#include "model_driver.hpp"

namespace model_test {
namespace {

using nnh::Entity;
using nnh::World;

Driver* g_driver = nullptr;

World make_world(int tags) {
    const nnh::ComponentDesc kinds[] = {
        nnh::component<MA>(kEntities, [](World&, Entity e, MA& c) { g_driver->world_hook(0, e.slot(), c.v); }),
        nnh::component<MB>(kEntities, [](World&, Entity e, MB& c) { g_driver->world_hook(1, e.slot(), c.v); }),
        nnh::component<MC>(kEntities, [](World&, Entity e, MC& c) { g_driver->world_hook(2, e.slot(), c.v); }),
    };
    return static_cast<World&&>(World::create({.max_entities = kEntities, .tags_per_entity = tags}, kinds).value());
}

template <typename T>
int value_at(World& w, int slot) {
    return w.storage<T>().data()[slot].v;
}

struct Visits {
    std::uint32_t h[kEntities * 2] = {};
    int n = 0;
    void add(std::uint32_t bits) {
        if (n < kEntities * 2) h[n++] = bits;
    }
    bool operator==(const Visits& o) const {
        if (n != o.n) return false;
        for (int i = 0; i < n; ++i)
            if (h[i] != o.h[i]) return false;
        return true;
    }
};

} // namespace

Driver::Driver(int tags) : world_((g_driver = this, make_world(tags))), model_(kEntities, tags, &Driver::model_hook) {}

Driver::~Driver() { g_driver = this; } // the world's final clear runs the hooks against this driver

void Driver::mix(std::uint32_t word) {
    for (int i = 0; i < 4; ++i) {
        sum_ ^= (word >> (8 * i)) & 0xFFu;
        sum_ *= 16777619u;
    }
}

void Driver::world_hook(int kind, int slot, int value) {
    (void)slot;
    if (kind == 1 && value % 3 == 0) world_.destroy(handles_[(value / 3) % kEntities]);
    if (kind == 0 && value % 11 == 0) world_.remove<MC>(handles_[(value / 11) % kEntities]);
    if (kind == 2 && value % 4 == 0 && world_.count() < kLiveLimit) {
        const Entity n = world_.spawn();
        if (n) {
            handles_[n.slot()] = n;
            world_.add<MA>(n, {value + 1});
        }
    }
}

void Driver::model_hook(world_model::Model& m, int kind, int slot, int value) {
    (void)slot;
    if (kind == 1 && value % 3 == 0) m.destroy(m.handle((value / 3) % kEntities));
    if (kind == 0 && value % 11 == 0) m.remove(2, m.handle((value / 11) % kEntities));
    if (kind == 2 && value % 4 == 0 && m.count() < kLiveLimit) {
        const std::uint32_t n = m.spawn();
        if (n != 0) m.add(0, n, value + 1);
    }
}

std::uint32_t Driver::random_handle_world(std::uint32_t arg) const { return handles_[arg % kEntities].bits(); }

bool Driver::compare_each(std::uint32_t op, std::uint32_t arg) {
    Visits w, m;
    const int kind_list[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 2}};
    const int pick = static_cast<int>(op % 3);
    const int destroy_mod = 5 + static_cast<int>(arg % 5);
    const auto value_of = [&](std::uint32_t h, int kind) {
        const world_model::Ent& e = model_.ent(static_cast<int>(h & 0xFFFF));
        return e.slot[kind] >= 0 ? model_.comp(kind, e.slot[kind]).value : -1;
    };
    if (pick == 0) {
        world_.each<MA>([&](Entity e, MA& a) {
            w.add(e.bits());
            if (a.v % destroy_mod == 0) world_.destroy(e);
        });
        model_.each(kind_list[0], 1, [&](std::uint32_t h) {
            m.add(h);
            if (value_of(h, 0) % destroy_mod == 0) model_.destroy(h);
        });
    } else if (pick == 1) {
        world_.each<MB, MA>([&](Entity e, MB& b, MA&) {
            w.add(e.bits());
            if (b.v % destroy_mod == 0) world_.remove<MA>(e);
            if (b.v % 6 == 1 && world_.count() < kLiveLimit) {
                const Entity n = world_.spawn();
                if (n) handles_[n.slot()] = n;
                world_.add<MB>(n, {b.v + 6});
                world_.add<MA>(n, {b.v});
            }
        });
        model_.each(kind_list[1], 2, [&](std::uint32_t h) {
            m.add(h);
            const int bv = value_of(h, 1);
            if (bv % destroy_mod == 0) model_.remove(0, h);
            if (bv % 6 == 1 && model_.count() < kLiveLimit) {
                const std::uint32_t n = model_.spawn();
                model_.add(1, n, bv + 6);
                model_.add(0, n, bv);
            }
        });
    } else {
        world_.each<MA, MB, MC>([&](Entity e, MA&, MB&, MC&) { w.add(e.bits()); });
        model_.each(kind_list[2], 3, [&](std::uint32_t h) { m.add(h); });
    }
    for (int i = 0; i < w.n; ++i) mix(w.h[i]);
    return w == m || fail("each visits differ");
}

bool Driver::compare_tags(std::uint32_t arg) {
    const std::uint32_t t = kTags[arg % 4];
    const std::uint32_t h = random_handle_world(arg >> 4);
    switch ((arg >> 12) % 5) {
    case 0:
        if (model_.tag_full(h, t)) return true; // the world would assert (tested in test_tags.cpp)
        if (world_.add_tag(Entity::from_bits(h), nnh::Tag{t}) != model_.add_tag(h, t)) return fail("add_tag");
        return true;
    case 1:
        if (world_.remove_tag(Entity::from_bits(h), nnh::Tag{t}) != model_.remove_tag(h, t)) return fail("remove_tag");
        return true;
    case 2: {
        Entity out[8];
        std::uint32_t mout[8];
        const int cap = static_cast<int>((arg >> 16) % 9);
        const int a = world_.find(nnh::Tag{t}, out, cap);
        const int b = model_.find(t, mout, cap);
        mix(static_cast<std::uint32_t>(a));
        if (a != b) return fail("find total");
        for (int i = 0; i < cap && i < a; ++i) {
            mix(out[i].bits());
            if (out[i].bits() != mout[i]) return fail("find order");
        }
        std::uint32_t first = 0;
        model_.find(t, &first, 1);
        if (world_.first(nnh::Tag{t}).bits() != first) return fail("first");
        return true;
    }
    case 3: {
        Visits w, m;
        world_.each_tagged(nnh::Tag{t}, [&](Entity e) {
            w.add(e.bits());
            if ((e.bits() >> 16) % 3 == 0) world_.destroy(e);
        });
        model_.each_entity(t, [&](std::uint32_t e) {
            m.add(e);
            if ((e >> 16) % 3 == 0) model_.destroy(e);
        });
        return w == m || fail("each_tagged");
    }
    default: {
        Visits w, m;
        for (Entity e : world_.entities()) {
            w.add(e.bits());
            if (w.n == 3 && world_.count() < kLiveLimit) {
                for (int i = 0; i < 2; ++i) {
                    const Entity n = world_.spawn();
                    if (n) handles_[n.slot()] = n;
                }
            }
        }
        model_.each_entity(0, [&](std::uint32_t e) {
            m.add(e);
            if (m.n == 3 && model_.count() < kLiveLimit) {
                model_.spawn();
                model_.spawn();
            }
        });
        for (int i = 0; i < w.n; ++i) mix(w.h[i]);
        return w == m || fail("entities()");
    }
    }
}

bool Driver::step(std::uint32_t op, std::uint32_t arg) {
    g_driver = this;
    // Keep room for the spawns and adds of one operation (a full world or kind asserts): flush when it runs low.
    bool low = model_.used(-1) > kEntities - 48;
    for (int k = 0; k < 3; ++k) low = low || model_.used(k) > kEntities - 48;
    if (low) {
        world_.flush();
        model_.flush();
    }
    const std::uint32_t h = random_handle_world(arg);
    const int kind = static_cast<int>((arg >> 8) % 3);
    const int value = static_cast<int>((arg >> 12) & 0xFF);
    bool ok = true;
    switch (op % 16) {
    case 0:
    case 1:
    case 2:
        if (model_.count() < kLiveLimit && model_.has_free_entity()) {
            const Entity e = world_.spawn();
            const std::uint32_t m = model_.spawn();
            mix(e.bits());
            if (e.bits() != m) return fail("spawn");
            handles_[e.slot()] = e;
        }
        break;
    case 3:
    case 4:
        ok = world_.destroy(Entity::from_bits(h)) == model_.destroy(h);
        if (!ok) return fail("destroy");
        break;
    case 5:
    case 6: {
        const int s = model_.live_slot(h);
        if (s < 0 || model_.ent(s).slot[kind] >= 0 || !model_.has_free(kind)) break;
        const Entity e = Entity::from_bits(h);
        const bool w = kind == 0   ? world_.add<MA>(e, {value}) != nullptr
                       : kind == 1 ? world_.add<MB>(e, {value}) != nullptr
                                   : world_.add<MC>(e, {value}) != nullptr;
        if (w != model_.add(kind, h, value)) return fail("add");
        break;
    }
    case 7: {
        const Entity e = Entity::from_bits(h);
        const bool w = kind == 0 ? world_.remove<MA>(e) : kind == 1 ? world_.remove<MB>(e) : world_.remove<MC>(e);
        if (w != model_.remove(kind, h)) return fail("remove");
        break;
    }
    case 8:
        world_.flush();
        model_.flush();
        break;
    case 9:
        if (arg % 16 == 0) {
            world_.clear();
            model_.clear();
        } else {
            world_.flush();
            model_.flush();
        }
        break;
    case 10:
    case 11:
    case 12:
        if (!compare_each(op >> 4, arg)) return false;
        break;
    default:
        if (!compare_tags(arg)) return false;
        break;
    }
    return same();
}

bool Driver::same() const {
    World& w = const_cast<World&>(world_);
    if (w.count() != model_.count()) return const_cast<Driver*>(this)->fail("count");
    for (int i = 0; i < kEntities; ++i) {
        const world_model::Ent& e = model_.ent(i);
        const Entity h = Entity::from_bits(model_.handle(i));
        if (w.alive(h) != e.live) return const_cast<Driver*>(this)->fail("alive");
        if (!e.live) continue;
        for (int t = 0; t < 4; ++t)
            if (w.has_tag(h, nnh::Tag{kTags[t]}) != model_.has_tag(h.bits(), kTags[t]))
                return const_cast<Driver*>(this)->fail("has_tag");
    }
    const nnh::Storage<MA> sa = w.storage<MA>();
    const nnh::Storage<MB> sb = w.storage<MB>();
    const nnh::Storage<MC> sc = w.storage<MC>();
    const int extents[3] = {sa.extent(), sb.extent(), sc.extent()};
    for (int k = 0; k < 3; ++k) {
        if (extents[k] != model_.kind_extent(k)) return const_cast<Driver*>(this)->fail("kind extent");
        for (int c = 0; c < kEntities; ++c) {
            const world_model::Comp& m = model_.comp(k, c);
            const bool live = k == 0 ? sa.is_live(c) : k == 1 ? sb.is_live(c) : sc.is_live(c);
            const Entity owner = k == 0 ? sa.owner(c) : k == 1 ? sb.owner(c) : sc.owner(c);
            const int v = k == 0 ? value_at<MA>(w, c) : k == 1 ? value_at<MB>(w, c) : value_at<MC>(w, c);
            if (live != m.live) return const_cast<Driver*>(this)->fail("component live");
            if (v != m.value) return const_cast<Driver*>(this)->fail("component value");
            if (live && owner.bits() != model_.handle(m.owner)) return const_cast<Driver*>(this)->fail("owner");
        }
    }
    return true;
}

} // namespace model_test
