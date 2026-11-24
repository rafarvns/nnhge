#pragma once
// The hierarchy example's self-check (spec 022 AC-2, AC-6): a fixed script of 50 000 operations chosen by the engine's
// FNV-1a hash of a seed and the step (no rand), played on two worlds of 1024 entities — one filled by prefab hierarchy
// spawns (12 prefabs: children, nested children, a variant appending a child, a 32-entity swarm, hooks that spawn under
// themselves), the other by the equivalent hand-written spawn/add/add_tag/attach calls — interleaved with local
// transforms set from seeded integers, attaches and detaches in both modes (refusals counted), detach_children,
// destroys (cascading, also inside each_descendant and from remove hooks), node removals, flushes, rare clears and a
// pass every 64 operations, after which every node's world_now must equal its stored world on both sides. After every
// operation both sides must have returned the same words; every 1000 operations and after each clear their full states
// must hash the same. The checksum must be the same on every compiler and on the 3DS. Pure code: the unit tests run it
// (tests/hierarchy_example/test_self_check.cpp).

#include <cstddef>
#include <cstdint>

#include "self_check_sides.hpp"

/// The checksum the simulator recorded for the whole script, and at every 10 000 operations. A different value on any
/// system is a determinism hole (or a change of the script or of the modules: then record the new values from
/// pc-debug here and in examples/hierarchy/tests/check.nnhs).
inline constexpr std::uint32_t kHierarchyCheckGolden = 0x23DB5C36u; // pc-debug, WinLibs GCC
inline constexpr std::uint32_t kHierarchyCheckpoints[5] = {0xD22B93ECu, 0x9DB7C5E5u, 0x820ED1D5u, 0x5ED71B4Du,
                                                           0x23DB5C36u};

class HierarchySelfCheck {
public:
    static constexpr int kOperations = 50000;
    static constexpr int kCheckpointEvery = 10000;
    static constexpr int kStateEvery = 1000;
    static constexpr int kPassEvery = 64;

    /// What the script reached (so its tests can tell it isn't vacuous).
    struct Stats {
        int spawns = 0, misses = 0, hook_spawns = 0, d_hooks = 0, cascades = 0;
        int attaches = 0, refusals = 0, detaches = 0, removals = 0, walks = 0, clears = 0;
        int passes = 0, world_now_checks = 0, state_checks = 0, max_live = 0, max_depth = 0;
    };

    bool start();
    /// Runs the next operation on both sides. @return true when that was operation 10 000, 20 000, ...
    bool step();
    /// One operation chosen by `h` on both sides (the fuzz target's entry). False once the sides disagree.
    bool apply(std::uint32_t h);
    bool done() const { return step_ >= kOperations || failed_; }
    bool failed() const { return failed_; }
    int failed_step() const { return fail_step_; }
    int operations() const { return step_; }
    std::uint32_t checksum() const { return sum_; }
    Stats stats() const;

private:
    std::uint32_t hash(std::uint32_t salt) const;
    void mix(std::uint32_t word);
    void compare_states();
    void pass();

    hcheck::Side<true> prefab_;
    hcheck::Side<false> hand_;
    std::uint32_t sum_ = 2166136261u;
    int step_ = 0;
    Stats counts_{};
    bool failed_ = false;
    int fail_step_ = -1;
};

/// "hierarchy: <n> sum XXXXXXXX" (a checkpoint log line).
void format_hierarchy_checkpoint(char* out, std::size_t size, int operations, std::uint32_t sum);
/// "hierarchy: PASS XXXXXXXX", or "hierarchy: FAIL step <n> sides differ" / "hierarchy: FAIL sum XXXXXXXX expected ...".
void format_hierarchy_verdict(char* out, std::size_t size, const HierarchySelfCheck& check);
