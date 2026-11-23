#pragma once
// The coins of a level: where they are, what they are worth and which ones were taken. Fixed capacity; the only
// engine call is collision::boxes_overlapping over the coins' own array (tested in tests/platformer/test_coin_book.cpp).
// Each coin carries collision layers (spec 019): layers::kCoin while it is there, kNoLayers once taken, so a taken
// coin is invisible to every query without a separate flag.

#include "layers.hpp"

#include <nnh/collision.hpp>
#include <nnh/math.hpp>

class CoinBook {
  public:
    static constexpr int kCapacity = 128;

    struct Coin {
        nnh::Rect bounds;
        int value = 1;
        nnh::collision::Layers layers = layers::kCoin; ///< kNoLayers once taken
    };

    /// Forgets every coin.
    void clear();
    /// Adds a coin (not taken). False when the book is full.
    bool add(nnh::Rect bounds, int value);
    /// Puts every coin back (a level restarts).
    void restart();
    /// Takes every coin still there that `box` overlaps (shared area > 0: touching edges don't count) when `mask`
    /// detects coins, in ascending index. Writes up to `capacity` of their indices into `taken` and returns how many
    /// were taken (all of them are, also past `capacity`).
    int collect(nnh::Rect box, nnh::collision::Layers mask, int* taken, int capacity);

    int count() const { return count_; }
    int collected() const { return collected_; }
    bool taken(int index) const {
        return index >= 0 && index < count_ && coins_[index].layers == nnh::collision::kNoLayers;
    }
    const Coin& coin(int index) const { return coins_[index]; }

  private:
    Coin coins_[kCapacity] = {};
    int count_ = 0;
    int collected_ = 0;
};
