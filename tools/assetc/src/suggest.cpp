// "Did you mean ...?" (suggest.hpp).

#include "suggest.hpp"

#include <algorithm>

namespace nnh::assetc {

std::size_t edit_distance(std::string_view a, std::string_view b) {
    // Three rows of the optimal string alignment table: the previous two and the current one.
    const std::size_t w = b.size() + 1;
    std::vector<std::size_t> before(w), previous(w), row(w);
    for (std::size_t j = 0; j < w; ++j)
        previous[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        row[0] = i;
        for (std::size_t j = 1; j < w; ++j) {
            const std::size_t change = a[i - 1] == b[j - 1] ? 0 : 1;
            row[j] = std::min({previous[j] + 1, row[j - 1] + 1, previous[j - 1] + change});
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                row[j] = std::min(row[j], before[j - 2] + 1); // two neighbours swapped
            }
        }
        before.swap(previous);
        previous.swap(row);
    }
    return previous[b.size()];
}

std::string closest(std::string_view typed, const std::vector<std::string>& names) {
    const std::size_t reach = std::max<std::size_t>(1, typed.size() / 3);
    const std::string* best = nullptr;
    std::size_t best_distance = reach + 1;
    for (const std::string& name : names) {
        const std::size_t d = edit_distance(typed, name);
        if (d < best_distance) {
            best = &name;
            best_distance = d;
        }
    }
    return best != nullptr ? *best : std::string();
}

} // namespace nnh::assetc
