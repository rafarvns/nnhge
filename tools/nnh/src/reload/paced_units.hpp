#pragma once
// The bytes a reload cycle still has to send (spec 031, review B1): whole wire units only — a message with its payload —
// so that other messages (typed commands, a script's input) sent between two slices land on a unit boundary and the
// game's reader stays aligned. A read offset instead of erasing from the front (a batch may be 32 MiB). The units after
// mark_tail() (a script's Resume) survive drop_batch(): a batch refused mid-send drops only what is left of it.
// Tests: tools/nnh/tests/test_reload_controller.cpp.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nnh::tool::reload {

class PacedUnits {
public:
    void clear();
    /// Appends `bytes`, well-formed messages each followed by its payload (the encoders' output).
    void append(const std::vector<std::uint8_t>& bytes);
    /// Units appended from now on are the tail.
    void mark_tail() { tail_unit_ = ends_.size(); }
    /// Whole units, as many as fit in `allowance` (decreased by what was taken).
    std::vector<std::uint8_t> take(std::size_t& allowance);
    /// Forgets the units not sent yet before the tail.
    void drop_batch();
    bool empty() const { return next_ == ends_.size(); }
    std::size_t units_left() const { return ends_.size() - next_; }

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<std::size_t> ends_; ///< each unit's end offset in bytes_
    std::size_t next_ = 0;          ///< the next unit to send
    std::size_t tail_unit_ = static_cast<std::size_t>(-1);
};

} // namespace nnh::tool::reload
