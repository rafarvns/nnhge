// Data folder name from the game title (see data_folder.hpp).

#include "app/data_folder.hpp"

#include <cstring>

namespace nnh::detail {

void derive_data_folder(const char* title, char* out, std::size_t capacity) {
    std::size_t n = 0;
    for (const char* p = title != nullptr ? title : ""; *p != '\0' && n + 1 < capacity; ++p) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c == ' ') c = '-';
        const bool keep = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (keep) out[n++] = c;
    }
    out[n] = '\0';
    if (n == 0) std::strncpy(out, "game", capacity - 1), out[capacity - 1] = '\0';
}

} // namespace nnh::detail
