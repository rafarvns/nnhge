// The platformer's coins (examples/platformer/coin_book.hpp): taken once by overlap, back on restart.

#include "platformer/coin_book.hpp"
#include "test.hpp"

namespace {

CoinBook two_coins() {
    CoinBook book;
    book.add({100, 50, 12, 12}, 1);
    book.add({120, 50, 12, 12}, 5);
    return book;
}

} // namespace

NNH_TEST(coin_book_starts_empty) {
    CoinBook book;
    CHECK(book.count() == 0);
    CHECK(book.collected() == 0);
}

NNH_TEST(coin_book_keeps_coins_in_the_order_they_were_added) {
    const CoinBook book = two_coins();
    CHECK(book.count() == 2);
    CHECK(book.coin(1).bounds.x == 120);
    CHECK(book.coin(1).value == 5);
    CHECK(!book.taken(0));
}

NNH_TEST(coin_book_takes_a_coin_the_box_overlaps) {
    CoinBook book = two_coins();
    int taken[4] = {-1, -1, -1, -1};
    CHECK(book.collect({95, 55, 10, 14}, taken, 4) == 1);
    CHECK(taken[0] == 0);
    CHECK(book.taken(0));
    CHECK(!book.taken(1));
    CHECK(book.collected() == 1);
}

NNH_TEST(coin_book_takes_a_coin_only_once) {
    CoinBook book = two_coins();
    int taken[4];
    book.collect({95, 55, 10, 14}, taken, 4);
    CHECK(book.collect({95, 55, 10, 14}, taken, 4) == 0);
    CHECK(book.collected() == 1);
}

NNH_TEST(coin_book_ignores_a_box_that_only_touches_a_coin) {
    CoinBook book = two_coins();
    int taken[4];
    CHECK(book.collect({90, 50, 10, 14}, taken, 4) == 0); // right edge at the coin's left edge
}

NNH_TEST(coin_book_takes_every_coin_the_box_overlaps_in_one_frame) {
    CoinBook book = two_coins();
    int taken[4] = {-1, -1, -1, -1};
    CHECK(book.collect({105, 50, 20, 14}, taken, 4) == 2);
    CHECK(taken[0] == 0);
    CHECK(taken[1] == 1);
}

NNH_TEST(coin_book_writes_no_more_indices_than_the_capacity) {
    CoinBook book = two_coins();
    int taken[2] = {-1, -1};
    CHECK(book.collect({105, 50, 20, 14}, taken, 1) == 2);
    CHECK(taken[0] == 0);
    CHECK(taken[1] == -1);
    CHECK(book.taken(1)); // taken all the same
}

NNH_TEST(coin_book_restart_puts_every_coin_back) {
    CoinBook book = two_coins();
    int taken[4];
    book.collect({105, 50, 20, 14}, taken, 4);
    book.restart();
    CHECK(!book.taken(0));
    CHECK(!book.taken(1));
    CHECK(book.collected() == 0);
    CHECK(book.count() == 2);
}

NNH_TEST(coin_book_refuses_coins_past_its_capacity) {
    CoinBook book;
    for (int i = 0; i < CoinBook::kCapacity; ++i)
        CHECK(book.add({static_cast<float>(i * 16), 0, 12, 12}, 1));
    CHECK(!book.add({0, 100, 12, 12}, 1));
    CHECK(book.count() == CoinBook::kCapacity);
}

NNH_TEST(coin_book_clear_forgets_every_coin) {
    CoinBook book = two_coins();
    book.clear();
    CHECK(book.count() == 0);
    CHECK(!book.taken(0));
}
