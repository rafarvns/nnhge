// Generational handles (spec 020 FR-1; T1.1): 16-bit slot + 16-bit generation in one word, 0 = null.

#include "nnh/detail/handle.hpp"
#include "nnh/detail/slot_bits.hpp"
#include "test.hpp"

#include <type_traits>

namespace {
struct Thing {};
using H = nnh::Handle<Thing>;
} // namespace

static_assert(sizeof(H) == 4);
static_assert(std::is_trivially_copyable_v<H>);

NNH_TEST(handle_value_initialised_is_null) {
    constexpr H h{};
    CHECK(!h);
    CHECK(h.bits() == 0);
    CHECK(h.slot() == 0 && h.generation() == 0);
}

NNH_TEST(handle_packs_slot_low_and_generation_high) {
    const H h = H::from_bits(0x0003002Au);
    CHECK(h.slot() == 42);
    CHECK(h.generation() == 3);
    CHECK(static_cast<bool>(h));
    const H top = H::from_bits(0xFFFFFFFFu);
    CHECK(top.slot() == 65535 && top.generation() == 65535);
}

NNH_TEST(handle_compares_by_its_bits) {
    CHECK(H::from_bits(7) == H::from_bits(7));
    CHECK(H::from_bits(7) != H::from_bits(0x10007));
    CHECK(H{} == H::from_bits(0));
}

NNH_TEST(handle_of_builds_from_slot_and_generation) {
    const H h = nnh::detail::make_handle<Thing>(5, 9);
    CHECK(h.slot() == 5 && h.generation() == 9);
}

NNH_TEST(next_generation_steps_by_one_and_wraps_to_one) {
    using nnh::detail::next_generation;
    CHECK(next_generation(1) == 2);
    CHECK(next_generation(65534) == 65535);
    CHECK(next_generation(65535) == 1); // never 0: no live handle is ever null
    std::uint32_t g = 1;
    for (int i = 0; i < 65535; ++i) {
        g = next_generation(g);
        CHECK(g != 0);
    }
    CHECK(g == 1); // 65 535 steps come back to the start: the documented wrap
}
