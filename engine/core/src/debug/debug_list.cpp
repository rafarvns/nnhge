// Debug text recorded for the debug pass (see debug_list.hpp).

#include "debug/debug_list.hpp"

#include <cstring>

namespace nnh::detail {

void DebugList::reset(DebugText* entries, int capacity, char* text, std::size_t text_capacity) {
    entries_ = entries;
    capacity_ = capacity;
    text_ = text;
    text_capacity_ = text_capacity;
    clear();
}

void DebugList::clear() {
    count_ = 0;
    dropped_ = 0;
    text_used_ = 0;
}

bool DebugList::add(Screen screen, int x, int y, const char* text, Color color) {
    const std::size_t length = std::strlen(text) + 1;
    if (count_ >= capacity_ || text_used_ + length > text_capacity_) {
        ++dropped_;
        return false;
    }
    std::memcpy(text_ + text_used_, text, length);
    entries_[count_++] = DebugText{screen, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), color,
                                   static_cast<std::uint32_t>(text_used_)};
    text_used_ += length;
    return true;
}

} // namespace nnh::detail
