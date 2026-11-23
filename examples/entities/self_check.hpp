#pragma once
// The entities example's self-check (spec 020 AC-2, AC-6): a fixed script of 100 000 operations on the engine's real
// worlds and pools, chosen by the engine's FNV-1a hash of a seed and the step (no rand): spawns, destroys (also from
// remove hooks and inside iterations), adds and removes of three kinds, tags, find/first/each_tagged, flushes,
// clears, and acquires/releases/iterations of a 256-slot run-time pool. An FNV-1a checksum takes every returned
// handle's bits, every visiting order, every find result and the stats. It must be the same on every compiler and on
// the 3DS. Pure code (no drawing, no input): the unit tests run it (tests/entities_example/test_self_check.cpp).

#include <cstddef>
#include <cstdint>
#include <nnh/entities.hpp>
#include <nnh/pool.hpp>

/// The checksum the simulator recorded for the whole script, and at every 10 000 operations. A different value on any
/// system is a determinism hole (or a change of the script or of the modules: then record the new values from
/// pc-debug here and in examples/entities/tests/check.nnhs).
inline constexpr std::uint32_t kEntitiesCheckGolden = 0xC8FB9A41u; // pc-debug, WinLibs GCC
inline constexpr std::uint32_t kEntitiesCheckpoints[10] = {0x723FB297u, 0x3BB094CBu, 0x58530BE8u, 0x21AD333Du,
                                                           0xDC95F672u, 0xC5CFA4B7u, 0x284AFBEEu, 0xF1272C58u,
                                                           0x45824FCCu, 0xC8FB9A41u};

class EntitiesSelfCheck {
  public:
    static constexpr int kOperations = 100000;
    static constexpr int kCheckpointEvery = 10000;
    static constexpr int kEntities = 1024;
    static constexpr int kLiveLimit = 700;
    static constexpr int kPoolSize = 256;

    /// What the script reached (so its tests can tell it isn't vacuous).
    struct Stats {
        int spawns = 0, destroys = 0, hook_destroys = 0, hook_spawns = 0, clears = 0, visits = 0, finds = 0;
        int acquires = 0, pool_misses = 0, max_live = 0;
    };

    struct A {
        std::int32_t v = 0, w = 0, x = 0, y = 0;
    };
    struct B {
        std::int32_t v = 0, w = 0;
    };
    struct C {
        std::int32_t v = 0;
    };
    struct Item {
        std::int32_t v = 0;
    };

    /// Creates the world and the pool (false: out of memory).
    bool start();
    /// Runs the next operation. @return true when that was operation 10 000, 20 000, ...
    bool step();
    bool done() const { return step_ >= kOperations; }
    int operations() const { return step_; }
    std::uint32_t checksum() const { return sum_; }
    const Stats& stats() const { return stats_; }

    void on_b_removed(int value); ///< B's remove hook (through the world): destroys or spawns by value

  private:
    std::uint32_t hash(std::uint32_t salt) const;
    void mix(std::uint32_t word);
    void spawn_one(std::uint32_t h);
    nnh::Entity pick(std::uint32_t h) const;
    void world_op(int op, std::uint32_t h);
    void iterate(int op, std::uint32_t h);
    void tag_op(int op, std::uint32_t h);
    void pool_op(int op, std::uint32_t h);

    nnh::World world_;
    nnh::Pool<Item> pool_;
    nnh::Entity handles_[kEntities] = {};
    nnh::Handle<Item> items_[kPoolSize] = {};
    std::uint32_t sum_ = 2166136261u;
    int step_ = 0;
    Stats stats_;
};

/// "entities: <n> sum XXXXXXXX" (a checkpoint log line).
void format_entities_checkpoint(char* out, std::size_t size, int operations, std::uint32_t sum);
/// "entities: PASS XXXXXXXX", or "entities: FAIL sum XXXXXXXX expected XXXXXXXX".
void format_entities_verdict(char* out, std::size_t size, std::uint32_t sum);
