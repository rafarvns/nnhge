// nnh::mem — tagged allocations on top of platform regions (nnh/memory.hpp).
//
// Heap and Linear blocks carry a small header just before the returned pointer. Vram can't be written by
// the CPU, so its blocks are tracked in a side table instead.

#include "nnh/memory.hpp"

#include <cstdint>

#include "log/engine_channels.hpp"
#include "nnh/assert.hpp"
#include "nnh/log.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/platform/memory.hpp"

namespace nnh::mem {
namespace {

constexpr std::uint32_t kMagic = 0x4E4E484Du; // "NNHM"
constexpr std::size_t kMaxAlign = 4096;
constexpr int kOverflowSlot = kMaxTags - 1;   // shared by tags beyond the table

struct alignas(8) Header {
    std::uint32_t magic;
    std::uint32_t size;
    std::uint16_t offset; // bytes from the raw platform pointer to the user pointer
    std::uint8_t region;
    std::uint8_t tag_slot;
};

struct VramBlock {
    void* pointer = nullptr;
    std::size_t size = 0;
    int tag_slot = 0;
};

struct TagEntry {
    const MemTag* tag = nullptr;
    MemStats stats;
};

MemStats g_regions[kMemRegionCount];
std::size_t g_budgets[kMemRegionCount];
bool g_budget_set[kMemRegionCount];
bool g_budget_warned[kMemRegionCount];
TagEntry g_tags[kMaxTags];
VramBlock g_vram[kMaxVramAllocations];
int g_live = 0;

int index_of(MemRegion region) { return static_cast<int>(region); }

int slot_for(MemTag& tag) {
    if (tag.slot >= 0 && g_tags[tag.slot].tag == &tag) return tag.slot;
    for (int i = 0; i < kOverflowSlot; ++i) {
        if (g_tags[i].tag == nullptr) {
            g_tags[i].tag = &tag;
            tag.slot = i;
            return i;
        }
    }
    NNH_LOG_WARN(memory, "more than %d memory tags; \"%s\" counted as \"other\"", kOverflowSlot, tag.name);
    return kOverflowSlot;
}

std::size_t budget_of(MemRegion region) {
    const int r = index_of(region);
    return g_budget_set[r] ? g_budgets[r] : platform::region_capacity(region);
}

void count(MemRegion region, int slot, std::size_t size, bool add) {
    MemStats* stats[2] = {&g_regions[index_of(region)], &g_tags[slot].stats};
    for (MemStats* s : stats) {
        if (add) {
            s->bytes += size;
            ++s->count;
            if (s->bytes > s->peak) s->peak = s->bytes;
        } else {
            s->bytes -= size;
            --s->count;
        }
    }
    g_live += add ? 1 : -1;
    const std::size_t budget = budget_of(region);
    const int r = index_of(region);
    if (add && budget != 0 && g_regions[r].bytes > budget && !g_budget_warned[r]) {
        g_budget_warned[r] = true;
        NNH_LOG_WARN(memory, "%s over budget: %zu of %zu bytes", region_name(region), g_regions[r].bytes, budget);
    }
}

void* alloc_vram(MemTag& tag, std::size_t size, std::size_t align) {
    for (VramBlock& block : g_vram) {
        if (block.pointer != nullptr) continue;
        void* p = platform::region_alloc(MemRegion::Vram, size, align);
        if (p == nullptr) return nullptr;
        block = VramBlock{p, size, slot_for(tag)};
        count(MemRegion::Vram, block.tag_slot, size, true);
        return p;
    }
    NNH_LOG_WARN(memory, "more than %d live vram allocations", kMaxVramAllocations);
    return nullptr;
}

bool free_vram(void* pointer) {
    for (VramBlock& block : g_vram) {
        if (block.pointer != pointer) continue;
        platform::region_free(MemRegion::Vram, pointer);
        count(MemRegion::Vram, block.tag_slot, block.size, false);
        block = VramBlock{};
        return true;
    }
    return false;
}

} // namespace

const char* region_name(MemRegion region) {
    switch (region) {
    case MemRegion::Heap: return "heap";
    case MemRegion::Linear: return "linear";
    case MemRegion::Vram: return "vram";
    }
    return "?";
}

void* alloc(MemRegion region, MemTag& tag, std::size_t size, std::size_t align) {
    NNH_VERIFY(align != 0 && (align & (align - 1)) == 0 && align <= kMaxAlign, "bad alignment %zu", align);
    if (align < alignof(Header)) align = alignof(Header);
    void* result = nullptr;
    if (region == MemRegion::Vram) {
        result = alloc_vram(tag, size, align);
    } else {
        auto* raw = static_cast<unsigned char*>(platform::region_alloc(region, size + align + sizeof(Header), 8));
        if (raw != nullptr) {
            const auto start = reinterpret_cast<std::uintptr_t>(raw) + sizeof(Header);
            const std::uintptr_t user = (start + align - 1) & ~(static_cast<std::uintptr_t>(align) - 1);
            auto* header = reinterpret_cast<Header*>(user - sizeof(Header));
            const int slot = slot_for(tag);
            *header = Header{kMagic, static_cast<std::uint32_t>(size),
                             static_cast<std::uint16_t>(user - reinterpret_cast<std::uintptr_t>(raw)),
                             static_cast<std::uint8_t>(region), static_cast<std::uint8_t>(slot)};
            count(region, slot, size, true);
            result = reinterpret_cast<void*>(user);
        }
    }
    if (result == nullptr) {
        NNH_LOG_WARN(memory, "%s out of memory: %zu bytes for \"%s\"", region_name(region), size, tag.name);
    }
    return result;
}

void free(void* pointer) {
    if (pointer == nullptr || free_vram(pointer)) return;
    auto* header = reinterpret_cast<Header*>(static_cast<unsigned char*>(pointer) - sizeof(Header));
    NNH_VERIFY(header->magic == kMagic, "nnh::mem::free of a pointer not from nnh::mem::alloc (or freed twice)");
    header->magic = 0;
    const auto region = static_cast<MemRegion>(header->region);
    count(region, header->tag_slot, header->size, false);
    platform::region_free(region, static_cast<unsigned char*>(pointer) - header->offset);
}

} // namespace nnh::mem

void nnh::detail::mem_account(MemRegion region, MemTag& tag, std::size_t bytes, bool add) {
    mem::count(region, mem::slot_for(tag), bytes, add);
}

namespace nnh::mem {

MemStats region_stats(MemRegion region) { return g_regions[index_of(region)]; }

MemStats tag_stats(const MemTag& tag) {
    if (tag.slot >= 0 && g_tags[tag.slot].tag == &tag) return g_tags[tag.slot].stats;
    return MemStats{};
}

std::size_t region_capacity(MemRegion region) { return platform::region_capacity(region); }

void set_budget(MemRegion region, std::size_t bytes) {
    const int r = index_of(region);
    g_budgets[r] = bytes;
    g_budget_set[r] = true;
    g_budget_warned[r] = false;
}

int live_allocations() { return g_live; }

void report_leaks() {
    for (const TagEntry& entry : g_tags) {
        if (entry.tag == nullptr || entry.stats.count == 0) continue;
        NNH_LOG_WARN(memory, "leak: tag \"%s\" still owns %zu bytes in %zu allocations", entry.tag->name,
                     entry.stats.bytes, entry.stats.count);
    }
}

void reset() {
    for (int r = 0; r < kMemRegionCount; ++r) {
        g_regions[r] = {};
        g_budgets[r] = 0;
        g_budget_set[r] = g_budget_warned[r] = false;
    }
    for (TagEntry& entry : g_tags) {
        if (entry.tag != nullptr) const_cast<MemTag*>(entry.tag)->slot = -1;
        entry = {};
    }
    for (VramBlock& block : g_vram) block = {};
    g_live = 0;
}

} // namespace nnh::mem
