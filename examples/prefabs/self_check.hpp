#pragma once
// The prefabs example's self-check (spec 021 AC-2, AC-6): a fixed script of 50 000 operations, chosen by the engine's
// FNV-1a hash of a seed and the step (no rand), played on two worlds of 1024 entities — one filled by prefab spawns
// (16 prefabs with variants, hooks, placement members; overrides and positions per copy), the other by the equivalent
// hand-written spawn/add/add_tag calls from a table written out flattened by hand — interleaved with destroys (also
// from hooks and inside iterations), adds, removes, flushes and clears. After every operation both sides must have
// returned the same handles; every 1000 operations and after each clear their full states must hash the same. The
// checksum (every returned handle and those state hashes) must be the same on every compiler and on the 3DS. Pure code:
// the unit tests run it (tests/prefabs_example/test_self_check.cpp).

#include "self_check_sides.hpp"

#include <cstddef>
#include <cstdint>

/// The checksum the simulator recorded for the whole script, and at every 10 000 operations. A different value on any
/// system is a determinism hole (or a change of the script or of the modules: then record the new values from
/// pc-debug here and in examples/prefabs/tests/check.nnhs).
inline constexpr std::uint32_t kPrefabsCheckGolden = 0x7BB7901Eu; // pc-debug, WinLibs GCC
inline constexpr std::uint32_t kPrefabsCheckpoints[5] = {0x3EBCC2FDu, 0x821C8B95u, 0xCC194CA2u, 0xC8360424u,
                                                         0x7BB7901Eu};

class PrefabsSelfCheck {
public:
    static constexpr int kOperations = 50000;
    static constexpr int kCheckpointEvery = 10000;
    static constexpr int kStateEvery = 1000;

    /// What the script reached (so its tests can tell it isn't vacuous).
    struct Stats {
        int spawns = 0, misses = 0, hook_spawns = 0, suicides = 0, d_spawns = 0, overrides = 0, positions = 0;
        int clears = 0, visits = 0, state_checks = 0, max_live = 0;
    };

    /// Creates both worlds and the library (false: out of memory).
    bool start();
    /// Runs the next operation on both sides. @return true when that was operation 10 000, 20 000, ...
    bool step();
    /// One operation chosen by `h` on both sides, then the step count and (every 1000) the state comparison: the fuzz
    /// target's entry. False once the sides disagree.
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

    check::Side<true> prefab_;
    check::Side<false> hand_;
    std::uint32_t sum_ = 2166136261u;
    int step_ = 0;
    int clears_ = 0, overrides_ = 0, positions_ = 0, state_checks_ = 0, max_live_ = 0;
    bool failed_ = false;
    int fail_step_ = -1;
};

/// "prefabs: <n> sum XXXXXXXX" (a checkpoint log line).
void format_prefabs_checkpoint(char* out, std::size_t size, int operations, std::uint32_t sum);
/// "prefabs: PASS XXXXXXXX", or "prefabs: FAIL step <n> sides differ" / "prefabs: FAIL sum XXXXXXXX expected XXXXXXXX".
void format_prefabs_verdict(char* out, std::size_t size, const PrefabsSelfCheck& check);
