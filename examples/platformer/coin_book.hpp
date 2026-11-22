#pragma once
// The coins of a level: where they are, what they are worth and which ones were taken. Fixed capacity, no
// engine calls but collision::overlaps (tested in tests/platformer/test_coin_book.cpp).

#include <nnh/math.hpp>

class CoinBook {
  public:
    static constexpr int kCapacity = 128;

    struct Coin {
        nnh::Rect bounds;
        int value = 1;
    };

    /// Forgets every coin.
    void clear();
    /// Adds a coin (not taken). False when the book is full.
    bool add(nnh::Rect bounds, int value);
    /// Puts every coin back (a level restarts).
    void restart();
    /// Takes every coin still there that `box` overlaps (shared area > 0: touching edges don't count). Writes
    /// up to `capacity` of their indices into `taken` and returns how many were taken.
    int collect(nnh::Rect box, int* taken, int capacity);

    int count() const { return count_; }
    int collected() const { return collected_; }
    bool taken(int index) const { return index >= 0 && index < count_ && taken_[index]; }
    const Coin& coin(int index) const { return coins_[index]; }

  private:
    Coin coins_[kCapacity] = {};
    bool taken_[kCapacity] = {};
    int count_ = 0;
    int collected_ = 0;
};
