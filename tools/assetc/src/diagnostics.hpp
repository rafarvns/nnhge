#pragma once
// The problems found in one data file (spec 014 FR-9): each with its line, printed as `<shown path>:<line>:
// <reason>` sorted by line (stable), at most kMaxPrinted, then "... and N more". Tests:
// tools/assetc/tests/test_diagnostics.cpp.

#include <string>
#include <vector>

namespace nnh::assetc {

inline constexpr int kMaxPrinted = 20;

class Diagnostics {
public:
    void add(int line, std::string message);
    bool empty() const { return problems_.empty(); }
    int count() const { return static_cast<int>(problems_.size()); }

    /// Every problem, one line each, as the build prints them ("" when there are none).
    std::string text(const std::string& shown) const;

private:
    struct Problem {
        int line;
        std::string message;
    };
    std::vector<Problem> problems_;
};

} // namespace nnh::assetc
