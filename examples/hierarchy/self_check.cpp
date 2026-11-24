#include "self_check.hpp"

#include <cstdio>
#include <cstring>

using hcheck::Side;
using nnh::Entity;
using nnh::Node;
using nnh::Reparent;

namespace {

constexpr std::uint32_t kSeed = 0x0E57C0DEu ^ 0x022u;

const nnh::Vec2 kScales[8] = {{1, 1}, {-1, -1}, {0.5f, 0.5f}, {2, 2}, {-0.75f, -0.75f}, {2, 0.5f}, {-1, 1}, {0, 0}};

/// A local transform from the bits of `h`: quarter turns or k/1024 rad, positions k/4, scales from the table.
nnh::Transform2D seeded_local(std::uint32_t h) {
    nnh::Transform2D t;
    const int k = static_cast<int>((h >> 8) & 0x1FFF) - 4096;
    t.rotation = (h & 0x80000000u) != 0 ? static_cast<float>(k % 5) * nnh::kQuarterTurn : static_cast<float>(k) / 1024.0f;
    t.position = {static_cast<float>(static_cast<int>((h >> 13) % 1601) - 800) / 4.0f,
                  static_cast<float>(static_cast<int>((h >> 3) % 1601) - 800) / 4.0f};
    t.scale = kScales[(h >> 24) % 8];
    return t;
}

bool has_node(int def) { return def != hcheck::kPlain; }

template <bool kPrefab>
std::uint32_t operate(Side<kPrefab>& s, std::uint32_t h, int step, HierarchySelfCheck::Stats& c) {
    nnh::World& w = s.world();
    nnh::Hierarchy& t = s.tree();
    const int op = static_cast<int>(h % 32);
    const Entity a = s.handle(h >> 5), b = s.handle(h >> 17);
    const auto spawn = [&](Entity parent) {
        int def = static_cast<int>((h >> 9) % hcheck::kPrefabs);
        if (parent && !has_node(def)) def = hcheck::kBare;
        const nnh::Vec2 at{static_cast<float>((h >> 14) % 1024) / 4.0f, static_cast<float>((h >> 4) % 768) / 4.0f};
        const bool placed = hcheck::hand_def(def).placed && ((h >> 20) & 1u) != 0;
        const Entity made = s.spawn(def, placed ? &at : nullptr, parent);
        s.remember(made);
        return made.bits();
    };
    if (op < 6 || op == 26 || op == 27) return w.count() < hcheck::kLiveLimit ? spawn(Entity{}) : 0;
    if (op == 8 || op == 9) return w.count() < hcheck::kLiveLimit && w.has<Node>(a) ? spawn(a) : 1;
    if (op < 13) { // 6, 7, 10, 11, 12
        nnh::Transform2D* local = t.local(a);
        if (local == nullptr) return 2;
        *local = seeded_local(h * 2654435761u);
        return 3;
    }
    if (op < 16 || op >= 28) {
        const Reparent mode = ((h >> 29) & 1u) != 0 ? Reparent::KeepLocal : Reparent::KeepWorld;
        const nnh::AttachCheck why = t.can_attach(a, b, mode);
        if (why != nnh::AttachCheck::Ok) {
            if (kPrefab) ++c.refusals;
            return 0x100u + static_cast<std::uint32_t>(why);
        }
        if (kPrefab) ++c.attaches;
        return t.attach(a, b, mode) ? 4u : 5u;
    }
    if (op == 16) {
        if (!w.has<Node>(a)) return 6;
        if (kPrefab) ++c.detaches;
        return t.detach(a, ((h >> 29) & 1u) != 0 ? Reparent::KeepLocal : Reparent::KeepWorld) ? 7u : 8u;
    }
    if (op == 17) return w.has<Node>(a) ? static_cast<std::uint32_t>(t.detach_children(a)) : 9u;
    if (op < 21) return w.destroy(a) ? 10u : 11u;
    if (op == 21) {
        if (!w.has<Node>(a)) return 12;
        if (kPrefab) ++c.walks;
        int n = 0;
        t.each_descendant(a, [&](Entity x) {
            s.mix(x.bits() ^ static_cast<std::uint32_t>(t.depth(x)));
            if (++n % 3 == 0) w.destroy(x);
        });
        return 0x200u + static_cast<std::uint32_t>(n);
    }
    if (op == 22) {
        const bool removed = w.remove<Node>(a);
        if (kPrefab && removed) ++c.removals;
        return removed ? 13u : 14u;
    }
    if (op == 25 && (h >> 8) % 128 == 0) {
        w.clear();
        s.flushed();
        return 15;
    }
    (void)step;
    w.flush();
    s.flushed();
    return 16;
}

/// Every node's world_now equals its stored world (bits); returns how many were checked, −1 on a difference.
template <bool kPrefab>
int check_world_now(Side<kPrefab>& s) {
    int n = 0;
    for (auto [e, node] : s.world().template storage<Node>().items()) {
        const nnh::WorldTransform2D now = s.tree().world_now(e);
        if (std::memcmp(&now, &node.world(), sizeof now) != 0) return -1;
        ++n;
    }
    return n;
}

} // namespace


bool HierarchySelfCheck::start() { return prefab_.start() && hand_.start(); }

std::uint32_t HierarchySelfCheck::hash(std::uint32_t salt) const {
    const std::uint32_t words[3] = {kSeed, static_cast<std::uint32_t>(step_), salt};
    char bytes[12];
    for (int i = 0; i < 12; ++i) bytes[i] = static_cast<char>((words[i / 4] >> (8 * (i % 4))) & 0xFFu);
    return nnh::detail::fnv1a32(bytes, sizeof bytes);
}

void HierarchySelfCheck::mix(std::uint32_t word) {
    for (int i = 0; i < 4; ++i) sum_ = (sum_ ^ ((word >> (8 * i)) & 0xFFu)) * 16777619u;
}

void HierarchySelfCheck::compare_states() {
    const std::uint32_t a = prefab_.state();
    ++counts_.state_checks;
    if (a != hand_.state()) failed_ = true;
    mix(a);
}

void HierarchySelfCheck::pass() {
    const int a = prefab_.tree().propagate();
    const int b = hand_.tree().propagate();
    const int na = check_world_now(prefab_), nb = check_world_now(hand_);
    if (a != b || na < 0 || nb < 0 || na != nb) failed_ = true;
    ++counts_.passes;
    counts_.world_now_checks += na > 0 ? na : 0;
    const nnh::HierarchyStats st = prefab_.tree().stats();
    if (st.deepest > counts_.max_depth) counts_.max_depth = st.deepest;
    mix(static_cast<std::uint32_t>(a));
    mix(static_cast<std::uint32_t>(st.roots));
}

bool HierarchySelfCheck::apply(std::uint32_t h) {
    Stats ignored{};
    const std::uint32_t a = operate(prefab_, h, step_, counts_);
    const std::uint32_t b = operate(hand_, h, step_, ignored);
    mix(a);
    if (a != b || prefab_.sum() != hand_.sum() || prefab_.misses() != hand_.misses()) failed_ = true;
    if (h % 32 == 25 && (h >> 8) % 128 == 0) {
        ++counts_.clears;
        compare_states();
    }
    if (prefab_.world().count() > counts_.max_live) counts_.max_live = prefab_.world().count();
    ++step_;
    if (step_ % kPassEvery == 0) pass();
    if (step_ % 50 == 0) { // keeps room for the next operations
        prefab_.world().flush();
        hand_.world().flush();
        prefab_.flushed();
        hand_.flushed();
    }
    if (step_ % kStateEvery == 0) compare_states();
    if (failed_ && fail_step_ < 0) fail_step_ = step_;
    return !failed_;
}

bool HierarchySelfCheck::step() {
    if (done()) return false;
    if (!apply(hash(0)) || step_ % kCheckpointEvery != 0) return false;
    mix(static_cast<std::uint32_t>(prefab_.world().count()));
    mix(static_cast<std::uint32_t>(prefab_.misses()));
    return true;
}

HierarchySelfCheck::Stats HierarchySelfCheck::stats() const {
    Stats s = counts_;
    s.spawns = prefab_.stats.spawns;
    s.misses = prefab_.misses();
    s.hook_spawns = prefab_.stats.hook_spawns;
    s.d_hooks = prefab_.stats.d_hooks;
    s.cascades = prefab_.stats.cascades;
    return s;
}

void format_hierarchy_checkpoint(char* out, std::size_t size, int operations, std::uint32_t sum) {
    std::snprintf(out, size, "hierarchy: %d sum %08X", operations, static_cast<unsigned>(sum));
}

void format_hierarchy_verdict(char* out, std::size_t size, const HierarchySelfCheck& check) {
    if (check.failed()) {
        std::snprintf(out, size, "hierarchy: FAIL step %d sides differ", check.failed_step());
    } else if (check.checksum() == kHierarchyCheckGolden) {
        std::snprintf(out, size, "hierarchy: PASS %08X", static_cast<unsigned>(check.checksum()));
    } else {
        std::snprintf(out, size, "hierarchy: FAIL sum %08X expected %08X", static_cast<unsigned>(check.checksum()),
                      static_cast<unsigned>(kHierarchyCheckGolden));
    }
}
