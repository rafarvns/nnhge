// Fuzz target bodies (see fuzz_targets.hpp).

#include "fuzz_targets.hpp"

#include <cstdlib>
#include <cstring>

#include "fs/save_format.hpp"
#include "nnh/fs.hpp"

namespace nnh::fuzz {

int path_target(const std::uint8_t* data, std::size_t size) {
    char path[512];
    const std::size_t n = size < sizeof path - 1 ? size : sizeof path - 1;
    std::memcpy(path, data, n);
    path[n] = '\0';
    (void)fs::validate_path(path);
    return 0;
}

int save_target(const std::uint8_t* data, std::size_t size) {
    const auto view = detail::validate_save(data, size);
    if (view) {
        // A valid view must point inside the input.
        const auto& v = view.value();
        if (v.payload < data || v.payload + v.size > data + size) std::abort();
    }
    return 0;
}

} // namespace nnh::fuzz
