// Cost and the plain loop (spec 031 T2.8, FR-17, FR-20): 100 000 attached frames without a batch allocate nothing and
// never walk the kinds; allocations happen only at the first batch and in apply frames; a plain `while (nnh::frame())`
// game with no reload-aware code applies batches.

#include "nnh/assets.hpp"
#include "nnh/memory.hpp"
#include "reload/reload_batch.hpp"
#include "reload/reload_kind.hpp"
#include "reload/reload_memory.hpp"
#include "reload_fixture.hpp"

using namespace reload_test;
using namespace nnh::detail::reload;

namespace {

const char kA[] = "a";

bool claim_all(const ReloadItem&) { return true; }
Prepared ok_prepare(const ReloadItem&) { return prepared_ok(); }
void no_op(const ReloadItem&, void*) {}
ReloadKind g_any{"any", claim_all, ok_prepare, no_op, no_op};

std::size_t heap_allocations() { return nnh::mem::region_stats(nnh::MemRegion::Heap).count; }

} // namespace

NNH_TEST(reload_cost_idle_frames_allocate_nothing_and_walk_nothing) {
    RELOAD_DEV_ONLY();
    char index[64];
    const Put assets[] = {text("a.txt", kA)};
    start(assets, 1, index, sizeof index);
    register_kind(g_any);
    const Put batch[] = {text("a.txt", "b")};
    send_batch(1, batch, 1);
    frames(2); // the first batch: buffer, overlay, block
    const std::size_t allocations = heap_allocations();
    const std::size_t reload_bytes = nnh::mem::tag_stats(reload_memory_tag()).bytes;
    const int calls = kind_calls();
    for (int i = 0; i < 100000; ++i) {
        if (!nnh::frame()) break;
    }
    CHECK(heap_allocations() == allocations);
    CHECK(nnh::mem::tag_stats(reload_memory_tag()).bytes == reload_bytes);
    CHECK(kind_calls() == calls);
    nnh::shutdown();
}

NNH_TEST(reload_cost_allocations_only_at_the_first_batch_and_in_apply_frames) {
    RELOAD_DEV_ONLY();
    char index[64];
    const Put assets[] = {text("a.txt", kA)};
    start(assets, 1, index, sizeof index);
    frames(3);
    CHECK(nnh::mem::tag_stats(reload_memory_tag()).bytes == 0); // nothing before a batch
    const Put batch[] = {text("a.txt", "b")};
    send_batch(1, batch, 1, 8);
    frames(1); // frame 5: received and staged (the buffer and the block)
    const std::size_t staged = nnh::mem::tag_stats(reload_memory_tag()).count;
    CHECK(staged >= 2);
    frames(2); // frames 6, 7: waiting, nothing new
    CHECK(nnh::mem::tag_stats(reload_memory_tag()).count == staged);
    frames(1); // frame 8: applied (the overlay, this frame's list)
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(nnh::mem::tag_stats(reload_memory_tag()).count > staged);
    nnh::shutdown();
    CHECK(nnh::mem::tag_stats(reload_memory_tag()).bytes == 0); // the shutdown hook freed everything
}

NNH_TEST(reload_cost_a_plain_loop_game_applies_batches) {
    RELOAD_DEV_ONLY();
    char index[64];
    const Put assets[] = {text("a.txt", kA)};
    start(assets, 1, index, sizeof index);
    const Put one[] = {text("a.txt", "one")};
    send_batch(1, one, 1, 5);
    const Put two[] = {text("a.txt", "two!")};
    fake::quit_after_polls(12);
    int frames_run = 0;
    while (nnh::frame()) { // the whole "game": no reload-aware code
        if (++frames_run == 6) send_batch(2, two, 1);
    }
    CHECK(nnh::assets::reload_count() == 2);
    char out[16];
    CHECK(read_text("a.txt", out, sizeof out) == 4);
    nnh::shutdown();
}
