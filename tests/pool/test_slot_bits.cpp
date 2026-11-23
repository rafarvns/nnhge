// Slot bitmaps (spec 020 T1.2): lowest free slot (live | quarantine clear) with a hint word, take, retire to
// quarantine, flush below the extent, fresh bits while iterating, lowest-bit scan. Checked at word boundaries and
// against a naive bit-array reference on 10 000 seeded operations.

#include "nnh/detail/slot_bits.hpp"
#include "test.hpp"

#include <cstdint>

using nnh::detail::lowest_bit;
using nnh::detail::SlotTable;

namespace {

struct Table {
    explicit Table(int capacity) {
        t.live = live;
        t.quarantine = quarantine;
        t.fresh = fresh;
        t.capacity = capacity;
    }
    std::uint32_t live[8] = {};
    std::uint32_t quarantine[8] = {};
    std::uint32_t fresh[8] = {};
    SlotTable t;
};

std::uint32_t lcg(std::uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

} // namespace

NNH_TEST(lowest_bit_finds_the_least_significant_set_bit) {
    CHECK(lowest_bit(1u) == 0);
    CHECK(lowest_bit(0x80000000u) == 31);
    CHECK(lowest_bit(0x00F00000u) == 20);
    CHECK(lowest_bit(0xFFFFFFFFu) == 0);
    for (int i = 0; i < 32; ++i) CHECK(lowest_bit((1u << i) | (i < 31 ? 0x80000000u : 0u)) == i);
}

NNH_TEST(slot_table_takes_the_lowest_free_slot) {
    Table tb(40);
    for (int i = 0; i < 3; ++i) {
        const int s = tb.t.find_free();
        CHECK(s == i);
        tb.t.take(s, false);
    }
    CHECK(tb.t.count == 3 && tb.t.extent == 3 && tb.t.peak == 3);
    CHECK(tb.t.is_live(2) && !tb.t.is_live(3));
}

NNH_TEST(slot_table_crosses_word_boundaries_31_32_33) {
    Table tb(70);
    for (int i = 0; i < 34; ++i) tb.t.take(tb.t.find_free(), false);
    CHECK(tb.t.is_live(31) && tb.t.is_live(32) && tb.t.is_live(33));
    CHECK(tb.t.extent == 34);
    tb.t.retire(32);
    tb.t.flush();
    CHECK(tb.t.find_free() == 32);
    tb.t.retire(31);
    tb.t.flush();
    CHECK(tb.t.find_free() == 31); // the hint went back below a freed word
}

NNH_TEST(slot_table_capacity_not_a_multiple_of_32_is_full_at_capacity) {
    Table tb(33);
    for (int i = 0; i < 33; ++i) {
        const int s = tb.t.find_free();
        CHECK(s == i);
        tb.t.take(s, false);
    }
    CHECK(tb.t.find_free() == -1); // bits 33..63 of the second word are never slots
    CHECK(tb.t.count == 33);
}

NNH_TEST(slot_table_quarantines_retired_slots_until_the_flush) {
    Table tb(8);
    for (int i = 0; i < 4; ++i) tb.t.take(tb.t.find_free(), false);
    tb.t.retire(1);
    CHECK(!tb.t.is_live(1));
    CHECK(tb.t.count == 3);
    CHECK(tb.t.find_free() == 4); // 1 is quarantined
    tb.t.flush();
    CHECK(tb.t.find_free() == 1);
    CHECK(tb.t.quarantine[0] == 0);
}

NNH_TEST(slot_table_marks_fresh_slots_while_iterating_and_clears_them_at_the_end) {
    Table tb(64);
    tb.t.take(tb.t.find_free(), false);
    tb.t.take(tb.t.find_free(), true);
    CHECK(tb.fresh[0] == 2u);
    tb.t.end_iteration();
    CHECK(tb.fresh[0] == 0u);
    CHECK(tb.t.is_live(1));
}

NNH_TEST(slot_table_reset_frees_everything_and_restarts_at_slot_zero) {
    Table tb(64);
    for (int i = 0; i < 40; ++i) tb.t.take(tb.t.find_free(), false);
    tb.t.retire(3);
    tb.t.reset();
    CHECK(tb.t.count == 0 && tb.t.extent == 0);
    CHECK(tb.live[0] == 0 && tb.live[1] == 0 && tb.quarantine[0] == 0);
    CHECK(tb.t.find_free() == 0);
}

NNH_TEST(slot_table_equals_a_naive_reference_on_10000_seeded_operations) {
    constexpr int kCap = 200;
    std::uint32_t live[7] = {}, quarantine[7] = {}, fresh[7] = {};
    SlotTable t;
    t.live = live;
    t.quarantine = quarantine;
    t.fresh = fresh;
    t.capacity = kCap;
    bool ref_live[kCap] = {}, ref_quar[kCap] = {};
    int ref_extent = 0;
    std::uint32_t seed = 12345;
    int mismatches = 0;
    for (int step = 0; step < 10000; ++step) {
        const std::uint32_t op = lcg(seed) % 10;
        if (op < 5) { // take
            int want = -1;
            for (int i = 0; i < kCap; ++i) {
                if (!ref_live[i] && !ref_quar[i]) {
                    want = i;
                    break;
                }
            }
            const int got = t.find_free();
            if (got != want) ++mismatches;
            if (got >= 0) {
                t.take(got, false);
                ref_live[got] = true;
                if (got + 1 > ref_extent) ref_extent = got + 1;
            }
        } else if (op < 9) { // retire a live slot
            const int s = static_cast<int>(lcg(seed) % kCap);
            if (ref_live[s]) {
                t.retire(s);
                ref_live[s] = false;
                ref_quar[s] = true;
            }
        } else {
            t.flush();
            for (bool& q : ref_quar) q = false;
        }
        int count = 0;
        for (int i = 0; i < kCap; ++i) {
            if (t.is_live(i) != ref_live[i]) ++mismatches;
            count += ref_live[i] ? 1 : 0;
        }
        if (t.count != count || t.extent != ref_extent) ++mismatches;
    }
    CHECK(mismatches == 0);
}
