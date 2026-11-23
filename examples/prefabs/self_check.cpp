#include "self_check.hpp"

#include <cstdio>

using check::Overrides;
using check::Side;
using nnh::Entity;

namespace {

constexpr std::uint32_t kSeed = 0x0E57C0DEu ^ 0x021u;

/// The copy's overrides and position from the bits of `h` (the same for both sides).
struct SpawnOrder {
    int def = 0;
    Overrides overrides;
    bool has_at = false;
    nnh::Vec2 at{};

    explicit SpawnOrder(std::uint32_t h) {
        def = static_cast<int>((h >> 5) % check::kPrefabs);
        overrides.count = static_cast<int>((h >> 9) % 3);
        overrides.kinds[0] = static_cast<int>((h >> 11) % check::kKinds);
        overrides.kinds[1] = (overrides.kinds[0] + 1 + static_cast<int>((h >> 14) % 4)) % check::kKinds;
        const auto x = static_cast<float>((h >> 16) & 0xFF), y = static_cast<float>((h >> 24) & 0x7F);
        overrides.v.a = {{x, y, 3, 5}, (h >> 3) & 0xFFu};
        overrides.v.b = check::b_seq(static_cast<int>(10 + ((h >> 18) & 7)));
        overrides.v.c = {{y, x}};
        overrides.v.d = check::D((h >> 20) & 0x3Fu);
        overrides.v.e = {static_cast<std::uint16_t>(h & 0xFFFu), static_cast<std::uint16_t>(h >> 22)};
        has_at = check::hand_def(def).place != check::Place::None && ((h >> 17) & 1u) != 0;
        at = {static_cast<float>((h >> 21) % 256), static_cast<float>((h >> 13) % 192)};
    }
};

/// One operation on one side; returns its result word (the same on both sides when they agree).
template <bool kPrefab>
std::uint32_t operate(Side<kPrefab>& s, std::uint32_t h, int step) {
    nnh::World& w = s.world();
    const int op = static_cast<int>(h % 32);
    const Entity e = s.handle(static_cast<int>(h >> 5));
    if (op < 12 || op >= 29) { // spawn
        if (w.count() >= check::kLiveLimit) return 0;
        const SpawnOrder order(h);
        const Entity made = s.spawn(order.def, &order.overrides, order.has_at ? &order.at : nullptr);
        s.remember(made);
        return made.bits();
    }
    if (op < 18) return w.destroy(e) ? 1u : 0u;
    if (op == 18 || op == 19) { // add C or E where there is room
        const int kind = op == 18 ? check::kC : check::kE;
        const bool has = kind == check::kC ? w.has<check::C>(e) : w.has<check::E>(e);
        if (!w.alive(e) || has || !s.room(kind)) return 2;
        const bool added = kind == check::kC ? w.add<check::C>(e, {{1, 2}}) != nullptr
                                             : w.add<check::E>(e, {7, static_cast<std::uint16_t>(step)}) != nullptr;
        if (added) s.took(kind);
        return added ? 1u : 0u;
    }
    if (op == 20) return w.remove<check::C>(e) ? 1u : 0u;
    if (op == 21) return w.remove<check::E>(e) ? 1u : 0u;
    if (op == 22) { // inside an iteration: a spawn (not visited) and destroys
        const int m = 5 + static_cast<int>((h >> 8) % 7);
        bool first = true;
        w.each<check::A>([&](Entity x, check::A& a) {
            s.mix(x.bits() ^ a.layers);
            ++s.stats.visits;
            if (first && w.count() < check::kLiveLimit) s.remember(s.spawn(static_cast<int>((h >> 12) % 16), nullptr, nullptr));
            first = false;
            if ((x.slot() + step) % m == 0) w.destroy(x);
        });
        return 3;
    }
    if (op == 23) {
        w.each<check::C, check::D>([&](Entity x, check::C& c, check::D& d) {
            s.mix(x.bits() ^ d.v ^ static_cast<std::uint32_t>(c.v.x));
            ++s.stats.visits;
        });
        return 4;
    }
    if (op == 24) {
        Entity found[8];
        const int total = w.find(check::kTags[(h >> 8) % 6], found, 8);
        for (int i = 0; i < total && i < 8; ++i) s.mix(found[i].bits());
        return static_cast<std::uint32_t>(total);
    }
    if (op == 25) {
        w.each_tagged(check::kTags[(h >> 8) % 6], [&](Entity x) {
            if (x.slot() % 4 == 1) w.destroy(x);
        });
        return 5;
    }
    if (op == 26 && (h >> 8) % 64 == 0) {
        w.clear();
        s.flushed();
        return 6;
    }
    w.flush();
    s.flushed();
    return 7;
}

} // namespace

bool PrefabsSelfCheck::start() { return prefab_.start() && hand_.start(); }

std::uint32_t PrefabsSelfCheck::hash(std::uint32_t salt) const {
    const std::uint32_t words[3] = {kSeed, static_cast<std::uint32_t>(step_), salt};
    char bytes[12];
    for (int i = 0; i < 12; ++i) bytes[i] = static_cast<char>((words[i / 4] >> (8 * (i % 4))) & 0xFFu);
    return nnh::detail::fnv1a32(bytes, sizeof bytes);
}

void PrefabsSelfCheck::mix(std::uint32_t word) {
    for (int i = 0; i < 4; ++i) sum_ = (sum_ ^ ((word >> (8 * i)) & 0xFFu)) * 16777619u;
}

void PrefabsSelfCheck::compare_states() {
    const std::uint32_t a = prefab_.state();
    ++state_checks_;
    if (a != hand_.state()) failed_ = true;
    mix(a);
}

bool PrefabsSelfCheck::apply(std::uint32_t h) {
    const int op = static_cast<int>(h % 32);
    if (op < 12 || op >= 29) {
        const SpawnOrder order(h);
        overrides_ += order.overrides.count;
        positions_ += order.has_at ? 1 : 0;
    }
    const std::uint32_t a = operate(prefab_, h, step_);
    const std::uint32_t b = operate(hand_, h, step_);
    mix(a);
    if (a != b || prefab_.sum() != hand_.sum() || prefab_.stats.misses != hand_.stats.misses) failed_ = true;
    if (op == 26 && (h >> 8) % 64 == 0) {
        ++clears_;
        compare_states();
    }
    if (step_ % 50 == 49) { // keeps room for the next operations
        prefab_.world().flush();
        hand_.world().flush();
        prefab_.flushed();
        hand_.flushed();
    }
    if (prefab_.world().count() > max_live_) max_live_ = prefab_.world().count();
    ++step_;
    if (step_ % kStateEvery == 0) compare_states();
    if (failed_ && fail_step_ < 0) fail_step_ = step_;
    return !failed_;
}

bool PrefabsSelfCheck::step() {
    if (done()) return false;
    if (!apply(hash(0)) || step_ % kCheckpointEvery != 0) return false;
    mix(static_cast<std::uint32_t>(prefab_.world().count()));
    mix(static_cast<std::uint32_t>(prefab_.library().stats().spawns));
    mix(static_cast<std::uint32_t>(prefab_.library().stats().misses));
    return true;
}

PrefabsSelfCheck::Stats PrefabsSelfCheck::stats() const {
    const check::SideStats& s = prefab_.stats;
    return {s.spawns, s.misses, s.hook_spawns, s.suicides, s.d_spawns, overrides_, positions_,
            clears_, s.visits, state_checks_, max_live_};
}

void format_prefabs_checkpoint(char* out, std::size_t size, int operations, std::uint32_t sum) {
    std::snprintf(out, size, "prefabs: %d sum %08X", operations, static_cast<unsigned>(sum));
}

void format_prefabs_verdict(char* out, std::size_t size, const PrefabsSelfCheck& check) {
    if (check.failed()) {
        std::snprintf(out, size, "prefabs: FAIL step %d sides differ", check.failed_step());
    } else if (check.checksum() == kPrefabsCheckGolden) {
        std::snprintf(out, size, "prefabs: PASS %08X", static_cast<unsigned>(check.checksum()));
    } else {
        std::snprintf(out, size, "prefabs: FAIL sum %08X expected %08X", static_cast<unsigned>(check.checksum()),
                      static_cast<unsigned>(kPrefabsCheckGolden));
    }
}
