// The open set (spec 025 FR-10, FR-12; T2.3): pops in the total order — (f, h) ascending, then the most recent push —
// for random keys with many ties, with the best-child buffer in front, and through compaction (stale entries dropped,
// the rest heapified) with a small capacity.

#include <algorithm>
#include <vector>

#include "path_fixture.hpp"
#include "pathfinding/open_heap.hpp"

using nnh::detail::OpenEntry;
using nnh::detail::OpenHeap;

namespace {

bool before(const OpenEntry& a, const OpenEntry& b) {
    if (a.f != b.f) return a.f < b.f;
    if (a.h != b.h) return a.h < b.h;
    return a.seq > b.seq;
}

OpenEntry random_entry(unsigned i, unsigned seq) {
    const unsigned r = path_test::hash4(i, 7, 0, 21);
    return {r % 5, 100 + (r >> 8) % 7, seq, r >> 16};
}

/// The reference: the remaining entries, the best taken each time.
OpenEntry take_best(std::vector<OpenEntry>& v) {
    auto it = std::min_element(v.begin(), v.end(), before);
    const OpenEntry e = *it;
    v.erase(it);
    return e;
}

bool same(const OpenEntry& a, const OpenEntry& b) { return a.f == b.f && a.h == b.h && a.seq == b.seq && a.cell == b.cell; }

} // namespace

NNH_TEST(path_open_heap_pops_10000_tied_keys_in_the_total_order) {
    std::vector<OpenEntry> storage(10000);
    OpenHeap heap;
    heap.data = storage.data();
    heap.capacity = 10000;
    std::vector<OpenEntry> ref;
    for (unsigned i = 0; i < 10000; ++i) {
        const OpenEntry e = random_entry(i, i);
        heap.push(e);
        ref.push_back(e);
    }
    std::sort(ref.begin(), ref.end(), before);
    OpenEntry e;
    for (const OpenEntry& want : ref) {
        CHECK(heap.take(e));
        CHECK(same(e, want));
    }
    CHECK(!heap.take(e));
}

NNH_TEST(path_open_heap_with_the_buffer_in_front_keeps_the_order) {
    std::vector<OpenEntry> storage(20000);
    OpenHeap heap;
    heap.data = storage.data();
    heap.capacity = 20000;
    std::vector<OpenEntry> ref;
    unsigned seq = 0;
    for (unsigned round = 0; round < 3000; ++round) {
        const unsigned children = path_test::hash4(round, 3, 0, 22) % 9; // an expansion: 0..8 pushes
        for (unsigned c = 0; c < children; ++c) {
            const OpenEntry e = random_entry(seq * 31 + c, seq);
            ++seq;
            heap.offer(e);
            ref.push_back(e);
        }
        if (ref.empty()) continue;
        OpenEntry got;
        CHECK(heap.take(got));
        CHECK(same(got, take_best(ref)));
    }
    OpenEntry got;
    while (!ref.empty()) {
        CHECK(heap.take(got));
        CHECK(same(got, take_best(ref)));
    }
    CHECK(!heap.take(got));
}

namespace {

/// Entries whose cell is odd are stale (a closed cell, an older g).
bool live(const void*, const OpenEntry& e) { return (e.cell & 1u) == 0; }

} // namespace

NNH_TEST(path_open_heap_compaction_drops_stale_entries_and_keeps_the_order) {
    std::vector<OpenEntry> storage(64);
    OpenHeap heap;
    heap.data = storage.data();
    heap.capacity = 64; // small: compacted many times
    std::vector<OpenEntry> ref; // live entries only
    unsigned seq = 0;
    int compactions = 0;
    for (unsigned round = 0; round < 2000; ++round) {
        const unsigned children = 1 + path_test::hash4(round, 4, 0, 23) % 4;
        for (unsigned c = 0; c < children; ++c) {
            if (heap.full()) {
                nnh::detail::open_compact(heap, live, nullptr);
                ++compactions;
                CHECK(!heap.full());
            }
            OpenEntry e = random_entry(seq * 17 + c, seq);
            ++seq;
            if (!live(nullptr, e) && c % 2 == 0) e.f += 1000; // most stale entries sink: they pile up
            heap.offer(e);
            if (live(nullptr, e)) ref.push_back(e);
        }
        // Pops skip stale entries, as the search does; the live count stays at most 20, stale entries pile up.
        while (ref.size() > 20) {
            OpenEntry got;
            do {
                CHECK(heap.take(got));
            } while (!live(nullptr, got));
            CHECK(same(got, take_best(ref)));
        }
    }
    CHECK(compactions > 10);
}

NNH_TEST(path_open_entries_are_16_bytes) { CHECK(sizeof(OpenEntry) == 16); }
