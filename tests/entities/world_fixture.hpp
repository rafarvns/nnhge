#pragma once
// Shared helpers for the world tests (spec 020 stages 2–5): small component kinds, a counting kind, world creation
// and the crash helpers for misuse.

#include "crash/crash_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/entities.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace world_test {

struct Pos {
    int x = 0;
    int y = 0;
};
struct Vel {
    int dx = 0;
    int dy = 0;
};
struct Hp {
    int value = 10;
};
struct alignas(32) Wide {
    int lanes[8] = {};
};

/// Counts constructions and destructions: they must balance.
struct Counted {
    static inline int constructed = 0;
    static inline int destroyed = 0;
    static int alive() { return constructed - destroyed; }
    static void reset() { constructed = destroyed = 0; }

    Counted() { ++constructed; }
    Counted(const Counted& o) : id(o.id) { ++constructed; }
    Counted& operator=(const Counted&) = default;
    ~Counted() { ++destroyed; }
    int id = 0;
};

/// A world with Pos, Vel and Hp (capacities `cap` each, at most `entities`) and `tags` tags per entity.
inline nnh::World make_world(int entities = 16, int cap = 16, int tags = 4) {
    if (cap > entities) cap = entities;
    const nnh::ComponentDesc kinds[] = {nnh::component<Pos>(cap), nnh::component<Vel>(cap),
                                        nnh::component<Hp>(cap)};
    auto r = nnh::World::create({.max_entities = entities, .tags_per_entity = tags}, kinds);
    CHECK(r.ok());
    return static_cast<nnh::World&&>(r.value());
}

inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install();
    return nnh::platform_fake::run_expecting_crash(fn);
}

/// Misuse asserts in Debug/Development and is harmless in Release.
inline bool asserts_or_is_ignored(void (*fn)()) { return crashes(fn) == nnh::build::kAssertsEnabled; }

/// After an expected assert inside an iteration or a hook (the crash path jumps out of it), marks the world idle.
void recover(nnh::World& world);

} // namespace world_test
