#include "coin_book.hpp"

#include <nnh/collision.hpp>

void CoinBook::clear() {
    count_ = 0;
    restart();
}

bool CoinBook::add(nnh::Rect bounds, int value) {
    if (count_ >= kCapacity) return false;
    coins_[count_] = {bounds, value};
    taken_[count_] = false;
    ++count_;
    return true;
}

void CoinBook::restart() {
    for (bool& taken : taken_)
        taken = false;
    collected_ = 0;
}

int CoinBook::collect(nnh::Rect box, int* taken, int capacity) {
    int found = 0;
    for (int i = 0; i < count_; ++i) {
        if (taken_[i] || !nnh::collision::overlaps(box, coins_[i].bounds)) continue;
        taken_[i] = true;
        ++collected_;
        if (found < capacity) taken[found] = i;
        ++found;
    }
    return found;
}
