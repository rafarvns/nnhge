#include "self_check_sides.hpp"

using nnh::ComponentValue;
using nnh::Entity;
using nnh::SpawnInfo;
using nnh::World;

namespace check {
namespace {

constexpr std::uint32_t kPrime = 16777619u;

std::uint32_t mix_bytes(std::uint32_t h, const void* data, std::size_t size) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) h = (h ^ p[i]) * kPrime;
    return h;
}

ComponentValue value_of(const Values& v, int kind) {
    switch (kind) {
    case kA: return nnh::component_value(v.a);
    case kB: return nnh::component_value(v.b);
    case kC: return nnh::component_value(v.c);
    case kD: return nnh::component_value(v.d);
    default: return nnh::component_value(v.e);
    }
}

void add_kind(World& w, Entity e, int kind, const Values& v) {
    switch (kind) {
    case kA: w.add<A>(e, v.a); break;
    case kB: w.add<B>(e, v.b); break;
    case kC: w.add<C>(e, v.c); break;
    case kD: w.add<D>(e, v.d); break;
    default: w.add<E>(e, v.e); break;
    }
}

int kind_of(nnh::KindId id) {
    if (id == nnh::kind_id<A>()) return kA;
    if (id == nnh::kind_id<B>()) return kB;
    if (id == nnh::kind_id<C>()) return kC;
    if (id == nnh::kind_id<D>()) return kD;
    return kE;
}

bool listed(const int* kinds, int count, int kind) {
    for (int i = 0; i < count; ++i)
        if (kinds[i] == kind) return true;
    return false;
}

template <typename T>
std::uint32_t mix_kind(World& w, std::uint32_t h) {
    const nnh::Storage<T> s = w.storage<T>();
    const int extent = s.extent();
    h = (h ^ static_cast<std::uint32_t>(extent)) * kPrime;
    for (int slot = 0; slot < extent; ++slot) {
        if (!s.is_live(slot)) continue;
        h = (h ^ static_cast<std::uint32_t>(slot)) * kPrime;
        h = (h ^ s.owner(slot).bits()) * kPrime;
        h = mix_bytes(h, s.data() + slot, sizeof(T));
    }
    return h;
}

} // namespace

template <bool kPrefab>
bool Side<kPrefab>::start() {
    Side* s = this;
    const nnh::ComponentDesc kinds[] = {
        nnh::component<A>(kCapacity[kA]), nnh::component<B>(kCapacity[kB]), nnh::component<C>(kCapacity[kC]),
        nnh::component<D>(kCapacity[kD], [s](World& w, Entity, D& d) { s->on_d_removed(w, d.v); }),
        nnh::component<E>(kCapacity[kE])};
    auto made = World::create({.max_entities = kEntities, .tags_per_entity = 4}, kinds);
    if (!made) return false;
    world_ = static_cast<World&&>(made.value());
    if constexpr (!kPrefab) return true;
    return register_prefabs();
}

template <bool kPrefab>
bool Side<kPrefab>::register_prefabs() {
    auto made = nnh::PrefabLibrary::create(
        {.max_prefabs = kPrefabs, .max_entries = 48, .value_bytes = 1024, .when_full = nnh::WhenFull::Miss});
    if (!made) return false;
    library_ = static_cast<nnh::PrefabLibrary&&>(made.value());
    Side* s = this;
    const nnh::SpawnHook info = [s](World& w, Entity e, const SpawnInfo& i) { s->run_hook(kHookInfo, w, e, i); };
    const nnh::SpawnHook mark = [s](World& w, Entity e, const SpawnInfo& i) { s->run_hook(kHookMark, w, e, i); };
    const nnh::SpawnHook child = [s](World& w, Entity e, const SpawnInfo& i) { s->run_hook(kHookChild, w, e, i); };
    const nnh::SpawnHook suicide = [s](World& w, Entity e, const SpawnInfo& i) {
        s->run_hook(kHookSuicide, w, e, i);
    };
    const nnh::Placement box = nnh::place(&A::box);
    const nnh::Placement vec = nnh::place(&C::v);
    const nnh::Tag* t = kTags;
    nnh::PrefabLibrary& l = library_;
    nnh::Prefab* p = prefabs_;
    bool ok = true;
    const auto keep = [&ok](nnh::Result<nnh::Prefab> r, nnh::Prefab& out) {
        ok = ok && r.ok();
        if (r) out = r.value();
    };
    keep(l.add("grunt", {.tags = {t[kEnemy]}, .place = box}, A{{0, 0, 8, 8}, 1}, b_seq(1)), p[0]);
    keep(l.add("coin", {.tags = {t[kCoin]}, .place = box}, A{{0, 0, 6, 6}, 2}, E{1, 2}), p[1]);
    keep(l.add("mover", {.tags = {t[kFx]}, .place = vec, .on_spawn = info}, C{{1, 2}}, D(7)), p[2]);
    keep(l.add("boss", {.tags = {t[kEnemy], t[kBoss]}, .place = box, .on_spawn = mark}, A{{0, 0, 16, 16}, 3},
               b_seq(2), D(9)),
         p[3]);
    keep(l.add("pickup", {.tags = {t[kPickup]}}, E{3, 4}), p[4]);
    keep(l.add("burst", {.on_spawn = child}), p[5]);
    keep(l.add("flying_grunt", {.base = p[0], .tags = {t[kFlying]}}, C{{5, 5}}), p[6]);
    keep(l.add("rich_coin", {.base = p[1]}, E{9, 9}), p[7]);
    keep(l.add("ace", {.base = p[6], .tags = {t[kBoss]}, .on_spawn = mark}, b_seq(3), D(3)), p[8]);
    keep(l.add("suicide", {.tags = {t[kFx]}, .place = box, .on_spawn = suicide}, A{{0, 0, 4, 4}, 5}, D(11)), p[9]);
    keep(l.add("dropper", {.base = p[2], .on_spawn = child}, E{6, 6}), p[10]);
    keep(l.add("wide", {.tags = {t[kEnemy], t[kCoin], t[kFx], t[kPickup]}, .place = vec}, A{{0, 0, 2, 2}, 6},
               b_seq(4), C{{7, 8}}, D(2), E{5, 6}),
         p[11]);
    keep(l.add("marked_dropper", {.base = p[10], .tags = {t[kBoss]}, .on_spawn = mark}), p[12]);
    keep(l.add("plain_e", {}, E{0, 1}), p[13]);
    keep(l.add("plain_bd", {}, b_seq(5), D(1)), p[14]);
    keep(l.add("spark", {.tags = {t[kFx]}, .place = vec}, C{{0, 0}}), p[15]);
    return ok;
}

template <bool kPrefab>
Entity Side<kPrefab>::spawn(int def, const Overrides* overrides, const nnh::Vec2* at) {
    const Entity e = kPrefab ? spawn_by_prefab(def, overrides, at) : spawn_by_hand(def, overrides, at);
    if (e) {
        ++stats.spawns;
    } else if (!world_.clearing()) {
        ++stats.misses;
    }
    return e;
}

template <bool kPrefab>
Entity Side<kPrefab>::spawn_by_prefab(int def, const Overrides* overrides, const nnh::Vec2* at) {
    ComponentValue list[2];
    const int count = overrides != nullptr ? overrides->count : 0;
    for (int i = 0; i < count; ++i) list[i] = value_of(overrides->v, overrides->kinds[i]);
    const Entity e = library_.spawn_with(world_, prefabs_[def], at, list, count);
    if (!e) return e;
    // The room bookkeeping, from the library's own answer (not the hand table).
    nnh::KindId ids[kKinds];
    const int n = library_.kinds(prefabs_[def], ids, kKinds);
    int kinds[kKinds];
    for (int i = 0; i < n; ++i) took(kinds[i] = kind_of(ids[i]));
    for (int i = 0; i < count; ++i)
        if (!listed(kinds, n, overrides->kinds[i])) took(overrides->kinds[i]);
    ++used_entities_;
    return e;
}

template <bool kPrefab>
Entity Side<kPrefab>::spawn_by_hand(int def, const Overrides* overrides, const nnh::Vec2* at) {
    if (world_.clearing()) return {};
    const HandDef& d = hand_def(def);
    int kinds[kKinds];
    int n = 0;
    for (int i = 0; i < d.kind_count; ++i) kinds[n++] = d.kinds[i];
    const int count = overrides != nullptr ? overrides->count : 0;
    for (int i = 0; i < count; ++i)
        if (!listed(d.kinds, d.kind_count, overrides->kinds[i])) kinds[n++] = overrides->kinds[i];
    bool fits = used_entities_ < kEntities;
    for (int i = 0; i < n && fits; ++i) fits = room(kinds[i]);
    if (!fits) return {};
    const Entity e = world_.spawn();
    ++used_entities_;
    for (int i = 0; i < n; ++i) {
        const bool overridden = count > 0 && listed(overrides->kinds, count, kinds[i]);
        add_kind(world_, e, kinds[i], overridden ? overrides->v : d.v);
        took(kinds[i]);
    }
    if (at != nullptr && d.place == Place::ABox) {
        world_.get<A>(e)->box.x = at->x;
        world_.get<A>(e)->box.y = at->y;
    } else if (at != nullptr && d.place == Place::CVec) {
        world_.get<C>(e)->v = *at;
    }
    for (int i = 0; i < d.tag_count; ++i) world_.add_tag(e, kTags[d.tags[i]]);
    SpawnInfo info{};
    info.has_position = at != nullptr;
    if (at != nullptr) info.position = *at;
    for (int i = 0; i < d.hook_count; ++i) run_hook(d.hooks[i], world_, e, info);
    return e;
}

template <bool kPrefab>
void Side<kPrefab>::run_hook(int hook, World& w, Entity e, const SpawnInfo& info) {
    if (hook == kHookInfo) {
        if (C* c = info.has_position ? w.get<C>(e) : nullptr) c->v = {info.position.x + 1.0f, info.position.y * 2.0f};
    } else if (hook == kHookMark) {
        if (A* a = w.get<A>(e)) a->layers ^= 0x5Au;
        if (D* d = w.get<D>(e)) d->v += 1;
    } else if (hook == kHookChild) {
        if (w.clearing() || w.count() >= kLiveLimit) return;
        const Entity c = spawn(kChild, nullptr, nullptr);
        mix(c.bits());
        remember(c);
        ++stats.hook_spawns;
    } else if (e.slot() % 3 == 0 && w.destroy(e)) {
        ++stats.suicides;
    }
}

template <bool kPrefab>
void Side<kPrefab>::on_d_removed(World& w, std::uint32_t value) {
    if (value % 7 != 3 || w.clearing() || w.count() >= kLiveLimit) return;
    const Entity e = spawn(kDSpawn, nullptr, nullptr);
    mix(e.bits());
    remember(e);
    ++stats.d_spawns;
}

template <bool kPrefab>
void Side<kPrefab>::mix(std::uint32_t word) {
    for (int i = 0; i < 4; ++i) sum_ = (sum_ ^ ((word >> (8 * i)) & 0xFFu)) * kPrime;
}

template <bool kPrefab>
void Side<kPrefab>::flushed() {
    used_entities_ = world_.count();
    used_[kA] = world_.storage<A>().count();
    used_[kB] = world_.storage<B>().count();
    used_[kC] = world_.storage<C>().count();
    used_[kD] = world_.storage<D>().count();
    used_[kE] = world_.storage<E>().count();
}

template <bool kPrefab>
std::uint32_t Side<kPrefab>::state() {
    std::uint32_t h = 2166136261u;
    for (const Entity e : world_.entities()) {
        h = (h ^ e.bits()) * kPrime;
        std::uint32_t tags = 0;
        for (int i = 0; i < 6; ++i) tags |= world_.has_tag(e, kTags[i]) ? 1u << i : 0u;
        h = (h ^ tags) * kPrime;
    }
    h = mix_kind<A>(world_, h);
    h = mix_kind<B>(world_, h);
    h = mix_kind<C>(world_, h);
    h = mix_kind<D>(world_, h);
    return mix_kind<E>(world_, h);
}

template class Side<true>;
template class Side<false>;

} // namespace check
