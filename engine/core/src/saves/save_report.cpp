// Field paths as text (FieldPath::format) and the load report's warning line (spec 027 FR-15).

#include <cstdio>

#include "nnh/saves.hpp"

namespace nnh::save {

int FieldPath::format(char* out, std::size_t capacity) const {
    if (capacity == 0) return 0;
    std::size_t n = 0;
    out[0] = '\0';
    for (int i = 0; i < count_; ++i) {
        char step[12];
        const bool item = is_item(i);
        const int length = std::snprintf(step, sizeof step, item ? "[%u]" : (i > 0 ? ".%u" : "%u"),
                                         static_cast<unsigned>(steps_[i]));
        for (int k = 0; k < length && n + 1 < capacity; ++k) out[n++] = step[k];
        out[n] = '\0';
    }
    return static_cast<int>(n);
}

} // namespace nnh::save
