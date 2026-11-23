// Fuzz target body for prefabs (spec 021 T5.5, see fuzz_targets.hpp): bytes become the prefabs self-check's operations
// (examples/prefabs/self_check.hpp), run on the prefab side and the hand-written side at once; any difference aborts.

#include "fuzz_targets.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#include "prefabs/self_check.hpp"

namespace nnh::fuzz {

int prefab_ops_target(const std::uint8_t* data, std::size_t size) {
    std::unique_ptr<PrefabsSelfCheck> check(new (std::nothrow) PrefabsSelfCheck());
    if (!check || !check->start()) return 0;
    for (std::size_t at = 0; at + 4 <= size; at += 4) {
        std::uint32_t h = 0;
        std::memcpy(&h, data + at, 4);
        if (!check->apply(h)) std::abort();
    }
    return 0;
}

} // namespace nnh::fuzz
