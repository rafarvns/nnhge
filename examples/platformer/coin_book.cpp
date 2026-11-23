#include "coin_book.hpp"

void CoinBook::clear() {
    count_ = 0;
    restart();
}

bool CoinBook::add(nnh::Rect bounds, int value) {
    if (count_ >= kCapacity) return false;
    coins_[count_] = {bounds, value, layers::kCoin};
    ++count_;
    return true;
}

void CoinBook::restart() {
    for (int i = 0; i < count_; ++i)
        coins_[i].layers = layers::kCoin;
    collected_ = 0;
}

int CoinBook::collect(nnh::Rect box, nnh::collision::Layers mask, int* taken, int capacity) {
    namespace collision = nnh::collision;
    int found[kCapacity];
    const collision::BoxView view = collision::box_view(coins_, count_, &Coin::bounds, &Coin::layers);
    const int count = collision::boxes_overlapping(view, box, found, kCapacity, {.mask = mask});
    for (int i = 0; i < count; ++i) {
        coins_[found[i]].layers = collision::kNoLayers; // taken: on no layers, no query finds it again
        ++collected_;
        if (i < capacity) taken[i] = found[i];
    }
    return count;
}
