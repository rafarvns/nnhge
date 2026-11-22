// The problems of one data file (diagnostics.hpp).

#include "diagnostics.hpp"

#include <algorithm>

namespace nnh::assetc {

void Diagnostics::add(int line, std::string message) {
    problems_.push_back({line, std::move(message)});
}

std::string Diagnostics::text(const std::string& shown) const {
    std::vector<Problem> sorted = problems_;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Problem& a, const Problem& b) { return a.line < b.line; });
    std::string out;
    const int printed = std::min(count(), kMaxPrinted);
    for (int i = 0; i < printed; ++i) {
        const Problem& p = sorted[static_cast<std::size_t>(i)];
        out += shown + ":" + std::to_string(p.line) + ": " + p.message + "\n";
    }
    if (count() > printed) out += shown + ": ... and " + std::to_string(count() - printed) + " more\n";
    return out;
}

} // namespace nnh::assetc
