// The asset index (asset_index.hpp).

#include "assets/asset_index.hpp"

#include <algorithm>
#include <cstring>

#include "assets/asset_path.hpp"

namespace nnh::detail {
namespace {

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool equal_ignoring_case(const char* a, const char* b) {
    for (; *a != '\0' && *b != '\0'; ++a, ++b) {
        if (lower(*a) != lower(*b)) return false;
    }
    return *a == *b;
}

bool less(const char* a, const char* b) { return std::strcmp(a, b) < 0; }

} // namespace

int AssetIndex::count_lines(const char* text, std::size_t size) {
    int lines = 0;
    bool in_line = false;
    for (std::size_t i = 0; i < size; ++i) {
        if (text[i] == '\n') {
            in_line = false;
        } else if (text[i] != '\r' && !in_line) {
            in_line = true;
            ++lines;
        }
    }
    return lines;
}

bool AssetIndex::parse(char* text, std::size_t size, const char** entries, int capacity) {
    entries_ = entries;
    count_ = 0;
    text[size] = '\0'; // the caller gives size + 1 bytes
    char* line = text;
    for (std::size_t i = 0; i <= size; ++i) {
        if (i < size && text[i] != '\n' && text[i] != '\r') continue;
        text[i] = '\0'; // lines are terminated in place
        if (*line != '\0') {
            if (count_ == capacity || !valid_asset_path(line)) {
                count_ = 0;
                return false;
            }
            entries_[count_++] = line;
        }
        line = text + i + 1;
    }
    std::sort(entries_, entries_ + count_, less);
    return true;
}

const char* AssetIndex::find(const char* path) const {
    if (path == nullptr || count_ == 0) return nullptr;
    const char** it = std::lower_bound(entries_, entries_ + count_, path, less);
    return it != entries_ + count_ && std::strcmp(*it, path) == 0 ? *it : nullptr;
}

const char* AssetIndex::find_ignoring_case(const char* path) const {
    if (path == nullptr) return nullptr;
    for (int i = 0; i < count_; ++i) {
        if (equal_ignoring_case(entries_[i], path)) return entries_[i];
    }
    return nullptr;
}

} // namespace nnh::detail
