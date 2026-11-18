// nnh::mem — tagged allocations, stats, budgets, leak report (fake platform regions).

#include <cstdint>
#include <cstring>

#include "memory/memory_internal.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::MemRegion;
namespace mem = nnh::mem;

NNH_MEM_TAG(testmem);
NNH_MEM_TAG(othermem);

namespace {

void fresh() {
    fake::reset();
    nnh::log::reset();
    mem::reset();
}

} // namespace

NNH_TEST(allocation_updates_region_and_tag_stats) {
    fresh();
    void* p = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(testmem), 100);
    CHECK(p != nullptr);
    CHECK(mem::region_stats(MemRegion::Heap).bytes == 100);
    CHECK(mem::region_stats(MemRegion::Heap).count == 1);
    CHECK(mem::tag_stats(NNH_MEM_TAG_REF(testmem)).bytes == 100);
    CHECK(mem::tag_stats(NNH_MEM_TAG_REF(othermem)).bytes == 0);
    mem::free(p);
}

NNH_TEST(free_returns_bytes_but_peak_remains) {
    fresh();
    void* a = mem::alloc(MemRegion::Linear, NNH_MEM_TAG_REF(testmem), 300);
    void* b = mem::alloc(MemRegion::Linear, NNH_MEM_TAG_REF(testmem), 200);
    mem::free(a);
    CHECK(mem::region_stats(MemRegion::Linear).bytes == 200);
    CHECK(mem::region_stats(MemRegion::Linear).peak == 500);
    mem::free(b);
    CHECK(mem::region_stats(MemRegion::Linear).bytes == 0);
    CHECK(mem::region_stats(MemRegion::Linear).count == 0);
}

NNH_TEST(peak_keeps_the_highest_usage_after_smaller_allocations) {
    fresh();
    void* big = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(testmem), 500);
    mem::free(big);
    void* small = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(testmem), 100);
    CHECK(mem::region_stats(MemRegion::Heap).peak == 500);
    CHECK(mem::tag_stats(NNH_MEM_TAG_REF(testmem)).peak == 500);
    mem::free(small);
}

NNH_TEST(pointers_respect_the_requested_alignment) {
    fresh();
    void* p = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(testmem), 10, 64);
    CHECK(reinterpret_cast<std::uintptr_t>(p) % 64 == 0);
    std::memset(p, 0xAB, 10); // the whole requested block is usable
    mem::free(p);
}

NNH_TEST(vram_allocations_are_tracked_in_a_side_table) {
    fresh();
    void* p = mem::alloc(MemRegion::Vram, NNH_MEM_TAG_REF(testmem), 4096, 128);
    CHECK(p != nullptr);
    CHECK(mem::region_stats(MemRegion::Vram).bytes == 4096);
    CHECK(fake::state().vram_writes_by_core == 0);
    mem::free(p);
    CHECK(mem::region_stats(MemRegion::Vram).bytes == 0);
}

NNH_TEST(going_over_budget_warns_once) {
    fresh();
    mem::set_budget(MemRegion::Heap, 1000);
    void* a = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(testmem), 800);
    const int before = fake::state().console_lines;
    void* b = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(testmem), 300);
    void* c = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(testmem), 300);
    CHECK(fake::state().console_lines == before + 1);
    CHECK(std::strstr(fake::state().last_console_line, "budget") != nullptr);
    mem::free(a);
    mem::free(b);
    mem::free(c);
}

NNH_TEST(platform_out_of_memory_returns_null_and_logs) {
    fresh();
    fake::set_region_capacity(MemRegion::Linear, 1024);
    void* p = mem::alloc(MemRegion::Linear, NNH_MEM_TAG_REF(testmem), 1 << 20);
    CHECK(p == nullptr);
    CHECK(std::strstr(fake::state().last_console_line, "out of memory") != nullptr);
    CHECK(mem::region_stats(MemRegion::Linear).bytes == 0);
}

NNH_TEST(leak_report_lists_live_allocations_per_tag) {
    fresh();
    void* p = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(othermem), 64);
    CHECK(mem::live_allocations() == 1);
    mem::report_leaks();
    CHECK(std::strstr(fake::state().last_console_line, "othermem") != nullptr);
    mem::free(p);
    CHECK(mem::live_allocations() == 0);
}

NNH_TEST(freeing_null_is_a_no_op) {
    fresh();
    mem::free(nullptr);
    CHECK(mem::live_allocations() == 0);
}

NNH_TEST(memory_owned_by_the_platform_can_be_accounted_under_a_tag) {
    fresh();
    nnh::detail::mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(testmem), 4096, true);
    CHECK(mem::region_stats(MemRegion::Linear).bytes == 4096);
    CHECK(mem::tag_stats(NNH_MEM_TAG_REF(testmem)).count == 1);
    CHECK(mem::live_allocations() == 1);
    nnh::detail::mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(testmem), 4096, false);
    CHECK(mem::region_stats(MemRegion::Linear).bytes == 0);
    CHECK(mem::region_stats(MemRegion::Linear).peak == 4096);
    CHECK(mem::live_allocations() == 0);
}
