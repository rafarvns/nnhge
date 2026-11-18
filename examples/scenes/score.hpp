#pragma once
// Counts coins by listening to CoinCollected: it doesn't know who publishes them.

#include <nnh/events.hpp>

#include "game_events.hpp"

class Score {
public:
    void start();  ///< starts listening (from 0)
    void stop();   ///< stops listening
    int value() const { return value_; }
    int coins() const { return coins_; }

private:
    void on_coin(const CoinCollected& coin);

    nnh::Subscription coin_sub_;
    int value_ = 0;
    int coins_ = 0;
};
