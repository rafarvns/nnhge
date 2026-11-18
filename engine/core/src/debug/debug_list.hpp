#pragma once
// Debug text recorded during the frame and drawn in the debug pass at its end, on top of the 2D passes
// (docs/architecture/rendering.md). Pure logic over caller-provided storage; unit tested in
// tests/debug/test_debug_list.cpp.

#include <cstddef>
#include <cstdint>

#include "nnh/gfx.hpp"

namespace nnh::detail {

struct DebugText {
    Screen screen = Screen::Top;
    std::int16_t x = 0;
    std::int16_t y = 0;
    Color color{};
    std::uint32_t offset = 0; ///< start of the text (nul-terminated) in the text buffer
};

class DebugList {
public:
    void reset(DebugText* entries, int capacity, char* text, std::size_t text_capacity);
    void clear();

    /// Copies `text`. @return false (and counts a drop) if the entries or the text buffer are full.
    bool add(Screen screen, int x, int y, const char* text, Color color);

    int count() const { return count_; }
    int dropped() const { return dropped_; }
    const DebugText& at(int i) const { return entries_[i]; }
    const char* text(int i) const { return text_ + entries_[i].offset; }

private:
    DebugText* entries_ = nullptr;
    int capacity_ = 0;
    char* text_ = nullptr;
    std::size_t text_capacity_ = 0;
    std::size_t text_used_ = 0;
    int count_ = 0;
    int dropped_ = 0;
};

} // namespace nnh::detail
