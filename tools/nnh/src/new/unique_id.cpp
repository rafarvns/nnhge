// A new project's .cia unique ID (unique_id.hpp).

#include "new/unique_id.hpp"

#include "new/project_name.hpp"

#include <random>

namespace nnh::tool {

std::uint32_t random_unique_id() {
    std::random_device source;
    std::uniform_int_distribution<std::uint32_t> pick(kUniqueIdMin, kUniqueIdMax);
    return pick(source);
}

} // namespace nnh::tool
