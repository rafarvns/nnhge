#pragma once
// Fixed-size ring of the most recent log lines (~10 KB). Unit tested in tests/log/test_ring.cpp.

#include <cstring>

#include "nnh/log.hpp"

namespace nnh::detail {

class LogRing {
public:
    void push(const char* line) {
        char* slot = lines_[(start_ + count_) % log::kRingLines];
        std::strncpy(slot, line, log::kMaxLineLength - 1);
        slot[log::kMaxLineLength - 1] = '\0';
        if (count_ < log::kRingLines) {
            ++count_;
        } else {
            start_ = (start_ + 1) % log::kRingLines;
        }
    }

    int size() const { return count_; }

    /// Line `index` counted from the oldest kept line; "" when out of range.
    const char* line(int index) const {
        if (index < 0 || index >= count_) return "";
        return lines_[(start_ + index) % log::kRingLines];
    }

    void clear() { start_ = count_ = 0; }

private:
    char lines_[log::kRingLines][log::kMaxLineLength] = {};
    int start_ = 0;
    int count_ = 0;
};

} // namespace nnh::detail
