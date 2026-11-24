// Fuzz target body for hierarchies (spec 022 T5.6, see fuzz_targets.hpp): bytes become the hierarchy self-check's
// operations (examples/hierarchy/self_check.hpp), run on the prefab-hierarchy side and the hand-written side at once;
// any difference (handles, sums, states, world_now after a pass) aborts.

#include "fuzz_targets.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#include "hierarchy/self_check.hpp"

namespace nnh::fuzz {

int hierarchy_ops_target(const std::uint8_t* data, std::size_t size) {
    std::unique_ptr<HierarchySelfCheck> check(new (std::nothrow) HierarchySelfCheck());
    if (!check || !check->start()) return 0;
    for (std::size_t at = 0; at + 4 <= size; at += 4) {
        std::uint32_t h = 0;
        std::memcpy(&h, data + at, 4);
        if (!check->apply(h)) std::abort();
    }
    return 0;
}

} // namespace nnh::fuzz
