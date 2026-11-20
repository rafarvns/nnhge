#include "pad.hpp"

namespace link_app {
namespace {

constexpr Pad::Key kKeys[Pad::kKeyCount] = {
    {'1', 0, 0, 1}, {'2', 1, 0, 1}, {'3', 2, 0, 1}, {Pad::kErase, 3, 0, 1},
    {'4', 0, 1, 1}, {'5', 1, 1, 1}, {'6', 2, 1, 1}, {'.', 3, 1, 1},
    {'7', 0, 2, 1}, {'8', 1, 2, 1}, {'9', 2, 2, 1}, {Pad::kOk, 3, 2, 2},
    {'0', 1, 3, 1},
};

} // namespace

const Pad::Key& Pad::key(int i) { return kKeys[i]; }

int Pad::index_at(int column, int row) const {
    for (int i = 0; i < kKeyCount; ++i) {
        const Key& k = kKeys[i];
        if (column == k.column && row >= k.row && row < k.row + k.rows) return enabled(i) ? i : -1;
    }
    return -1;
}

char Pad::hit(int x, int y) const {
    if (x < kLeft || y < kTop) return 0;
    const int column = (x - kLeft) / kKeyWidth;
    const int row = (y - kTop) / kKeyHeight;
    if (column >= kColumns || row >= kRows) return 0;
    const int i = index_at(column, row);
    return i < 0 ? 0 : kKeys[i].c;
}

void Pad::move(int dx, int dy) {
    const Key& from = kKeys[focus_];
    int column = from.column;
    int row = dy > 0 ? from.row + from.rows - 1 : from.row; // leave a tall key from its far edge
    for (;;) {
        column += dx;
        row += dy;
        if (column < 0 || column >= kColumns || row < 0 || row >= kRows || (dx == 0 && dy == 0)) return;
        const int i = index_at(column, row);
        if (i >= 0 && i != focus_) {
            focus_ = i;
            return;
        }
    }
}

} // namespace link_app
