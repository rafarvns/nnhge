#pragma once
// Shared helpers for the pool tests (spec 020 T1.3–T1.5): every behaviour case runs on the inline form
// (Pool<T, N>) and on the run-time form (Pool<T>::create(N)) through POOL_CASE.

#include "crash/crash_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/pool.hpp"
#include "test.hpp"

namespace pool_test {

struct Item {
    int value = 0;
    int other = 7;
};

/// Counts constructions and destructions: they must balance.
struct Counted {
    static inline int constructed = 0;
    static inline int destroyed = 0;
    static int alive() { return constructed - destroyed; }
    static void reset() { constructed = destroyed = 0; }

    Counted() { ++constructed; }
    Counted(const Counted& o) : id(o.id) { ++constructed; }
    Counted(Counted&& o) noexcept : id(o.id) {
        ++constructed;
        o.id = -1;
    }
    Counted& operator=(const Counted&) = default;
    ~Counted() { ++destroyed; }
    int id = 0;
};

/// Runs `fn` and reports whether it reached the crash screen (an assert).
inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install();
    return nnh::platform_fake::run_expecting_crash(fn);
}

/// Misuse asserts in Debug/Development and is harmless in Release.
inline bool asserts_or_is_ignored(void (*fn)()) { return crashes(fn) == nnh::build::kAssertsEnabled; }

/// After an expected assert inside an iteration (the crash path jumps out of it), ends the iteration by hand.
template <typename P>
void recover(P& pool) {
    nnh::detail::PoolCore& core = *nnh::detail::PoolAccess::core(pool);
    core.iterating = 0;
    core.slots.end_iteration();
    core.any_fresh = false;
}

template <typename T, int N>
using Inline = nnh::Pool<T, N>;

template <typename T>
nnh::Pool<T> runtime(int capacity) {
    auto r = nnh::Pool<T>::create(capacity);
    CHECK(r.ok());
    return static_cast<nnh::Pool<T>&&>(r.value());
}

} // namespace pool_test

/// A behaviour case run on both pool forms with capacity `cap` of `type`: write `POOL_CASE(name, Item, 8) { ...
/// pool ... }` (the body sees `pool` as a reference to either form).
#define POOL_CASE(name, type, cap)                                                                         \
    template <typename P>                                                                                  \
    static void name##_body(P& pool);                                                                      \
    NNH_TEST(name##_inline) {                                                                              \
        nnh::Pool<type, cap> pool;                                                                         \
        name##_body(pool);                                                                                 \
    }                                                                                                      \
    NNH_TEST(name##_runtime) {                                                                             \
        nnh::Pool<type> pool = pool_test::runtime<type>(cap);                                              \
        name##_body(pool);                                                                                 \
    }                                                                                                      \
    template <typename P>                                                                                  \
    static void name##_body(P& pool)
