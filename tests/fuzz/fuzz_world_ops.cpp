// Fuzz target body for worlds (spec 020 T4.4, see fuzz_targets.hpp): bytes become operations run on an nnh::World and
// on the reference model (tests/entities/world_model.hpp) at once; any difference aborts.

#include "fuzz_targets.hpp"

#include <cstdlib>
#include <cstring>

#include "model_driver.hpp"

namespace nnh::fuzz {

int world_ops_target(const std::uint8_t* data, std::size_t size) {
    if (size < 1) return 0;
    model_test::Driver driver(data[0] % 9);
    for (std::size_t at = 1; at + 8 <= size; at += 8) {
        std::uint32_t op = 0, arg = 0;
        std::memcpy(&op, data + at, 4);
        std::memcpy(&arg, data + at + 4, 4);
        if (!driver.step(op, arg)) std::abort();
    }
    return 0;
}

} // namespace nnh::fuzz
