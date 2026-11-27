// PC simulator: the Debug/Development data folder (see data_root_pc.hpp).

#include "data_root_pc.hpp"

#include <cstdio>

namespace nnh::platform::pc {

bool dev_data_root(const char* base, const char* folder, char* out, std::size_t size) {
    const int n = std::snprintf(out, size, "%snnh_data/%s/", base, folder);
    return n > 0 && static_cast<std::size_t>(n) < size;
}

} // namespace nnh::platform::pc
