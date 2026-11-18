#include "score.hpp"

void Score::start() {
    value_ = 0;
    coins_ = 0;
    coin_sub_ = nnh::events::subscribe<&Score::on_coin>(this);
}

void Score::stop() { coin_sub_.reset(); }

void Score::on_coin(const CoinCollected& coin) {
    value_ += coin.value;
    ++coins_;
}
