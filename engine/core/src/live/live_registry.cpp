// The live registry (see live_registry.hpp): the block, slots, generations and the two sorted indexes.

#include "live/live_registry.hpp"

#include <cstring>
#include <new>

#include "core/shutdown_hooks.hpp"
#include "debug/live_hook.hpp"
#include "live/live_names.hpp"
#include "live/live_ops.hpp"
#include "nnh/assert.hpp"
#include "nnh/memory.hpp"

namespace nnh {

NNH_MEM_TAG(live);

namespace detail::live {

Registry* g_registry = nullptr;
void (*g_on_removed)(std::uint32_t hash) = nullptr;
void (*g_on_added)(int ref, bool watch) = nullptr;

namespace {

int g_cap_tunables = 128, g_cap_watches = 64, g_cap_shown = 64;
std::uint16_t g_next_generation = 0; // shared by every registry, so a stale id never names a later entry

std::uint16_t new_generation() {
    if (++g_next_generation == 0) ++g_next_generation;
    return g_next_generation;
}

std::size_t align8(std::size_t n) { return (n + 7) & ~std::size_t{7}; }

void shutdown_hook() { destroy_registry(); }

int compare_names(const char* a, int an, const char* b, int bn) {
    const int n = an < bn ? an : bn;
    const int c = std::memcmp(a, b, static_cast<std::size_t>(n));
    return c != 0 ? c : an - bn;
}

const char* ref_name(const Registry& r, std::uint16_t ref, int& length) {
    if ((ref & kShownRef) != 0) {
        const ShownEntry& s = r.shown[ref & ~kShownRef];
        length = s.name_length;
        return s.name;
    }
    const WatchEntry& w = r.watches[ref];
    length = w.name_length;
    return w.name;
}

/// Inserts `value` into the sorted index of `count` items at its name's place.
template <typename NameOf>
void sorted_insert(std::uint16_t* index, int count, std::uint16_t value, const char* name, int length, NameOf name_of) {
    int lo = 0, hi = count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        int n = 0;
        const char* m = name_of(index[mid], n);
        if (compare_names(m, n, name, length) < 0) lo = mid + 1;
        else hi = mid;
    }
    std::memmove(index + lo + 1, index + lo, static_cast<std::size_t>(count - lo) * sizeof *index);
    index[lo] = value;
}

void sorted_erase(std::uint16_t* index, int count, std::uint16_t value) {
    for (int i = 0; i < count; ++i) {
        if (index[i] != value) continue;
        std::memmove(index + i, index + i + 1, static_cast<std::size_t>(count - i - 1) * sizeof *index);
        return;
    }
}

bool name_taken(const char* existing, int n, const char* name, int length, std::uint32_t hash, std::uint32_t h) {
    if (h == hash && compare_names(existing, n, name, length) != 0) {
        NNH_ASSERT(false, "live: \"%.*s\" and \"%.*s\" have the same hash: rename one", length, name, n, existing);
        return true;
    }
    if (compare_names(existing, n, name, length) == 0) {
        NNH_ASSERT(false, "live: \"%.*s\" is registered twice", length, name);
        return true;
    }
    if (in_group(existing, static_cast<std::size_t>(n), name, static_cast<std::size_t>(length)) ||
        in_group(name, static_cast<std::size_t>(length), existing, static_cast<std::size_t>(n))) {
        NNH_ASSERT(false, "live: \"%.*s\" and \"%.*s\": a name can't also be a group", length, name, n, existing);
        return true;
    }
    return false;
}

/// Whether the PC was told about an entry (an unsent one goes without a LiveRemoved record).
bool announced(Registry& r, std::uint8_t flags) {
    if ((flags & kUnsent) == 0) return true;
    --r.unsent_count;
    return false;
}

void drop_follows(Registry& r, std::uint32_t hash) {
    int kept = 0;
    for (int i = 0; i < r.follow_count; ++i) {
        if (r.follows[i].hash != hash) r.follows[kept++] = r.follows[i];
    }
    r.follow_count = kept;
}

} // namespace

std::size_t block_bytes(int t, int w, int s) {
    std::size_t n = align8(sizeof(Registry));
    n += align8(sizeof(Entry) * static_cast<std::size_t>(t));
    n += align8(sizeof(WatchEntry) * static_cast<std::size_t>(w));
    n += align8(sizeof(ShownEntry) * static_cast<std::size_t>(s));
    n += align8(sizeof(std::uint16_t) * static_cast<std::size_t>(t));
    n += align8(sizeof(std::uint16_t) * static_cast<std::size_t>(w + s));
    n += align8(sizeof(std::uint16_t) * static_cast<std::size_t>(shown_buckets_for(s)));
    return n;
}

void set_capacities(int max_tunables, int max_watches, int max_shown) {
    NNH_ASSERT(g_registry == nullptr, "live: configure() after the first registration");
    NNH_ASSERT(max_tunables >= 1 && max_tunables <= 4096 && max_watches >= 0 && max_watches <= 4096 && max_shown >= 0 &&
                   max_shown <= 4096,
               "live: configure() capacities must be 1..4096 tunables, 0..4096 watches and shown");
    if (g_registry != nullptr) return;
    g_cap_tunables = max_tunables;
    g_cap_watches = max_watches;
    g_cap_shown = max_shown;
}

Registry& registry() {
    if (g_registry != nullptr) return *g_registry;
    const std::size_t bytes = block_bytes(g_cap_tunables, g_cap_watches, g_cap_shown);
    auto* block = static_cast<std::uint8_t*>(mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(live), bytes));
    NNH_VERIFY(block != nullptr, "live: no memory for the registry (%u bytes)", static_cast<unsigned>(bytes));
    std::memset(block, 0, bytes);
    auto* r = new (block) Registry();
    std::size_t at = align8(sizeof(Registry));
    const auto take = [&](std::size_t size) {
        std::uint8_t* p = block + at;
        at += align8(size);
        return p;
    };
    r->max_tunables = g_cap_tunables;
    r->max_watches = g_cap_watches;
    r->max_shown = g_cap_shown;
    r->entries = new (take(sizeof(Entry) * static_cast<std::size_t>(r->max_tunables))) Entry[r->max_tunables];
    r->watches = new (take(sizeof(WatchEntry) * static_cast<std::size_t>(r->max_watches))) WatchEntry[r->max_watches];
    r->shown = new (take(sizeof(ShownEntry) * static_cast<std::size_t>(r->max_shown))) ShownEntry[r->max_shown];
    r->tunable_index = reinterpret_cast<std::uint16_t*>(take(sizeof(std::uint16_t) * static_cast<std::size_t>(r->max_tunables)));
    r->watch_index =
        reinterpret_cast<std::uint16_t*>(take(sizeof(std::uint16_t) * static_cast<std::size_t>(r->max_watches + r->max_shown)));
    r->shown_buckets = shown_buckets_for(r->max_shown);
    r->shown_table = reinterpret_cast<std::uint16_t*>(take(sizeof(std::uint16_t) * static_cast<std::size_t>(r->shown_buckets)));
    g_registry = r;
    add_shutdown_hook(shutdown_hook);
    install_ops();
    return *r;
}

void destroy_registry() {
    if (g_registry == nullptr) return;
    void* block = g_registry;
    g_registry->~Registry();
    g_registry = nullptr;
    uninstall_ops();
    mem::free(block);
    g_cap_tunables = 128;
    g_cap_watches = 64;
    g_cap_shown = 64;
}

bool name_usable(const Registry& r, const char* text, int length) {
    const live_name::Error e = length > nnh::live::kMaxNameBytes ? live_name::Error::TooLong
                                                                 : check_name(text, static_cast<std::size_t>(length));
    if (e != live_name::Error::None) {
        NNH_ASSERT(false, "live: name \"%.*s\" %s", length, text, name_error_text(e));
        return false;
    }
    const std::uint32_t hash = fnv1a32(text, static_cast<std::size_t>(length));
    for (int i = 0; i < r.tunable_count; ++i) {
        const Entry& x = r.entries[r.tunable_index[i]];
        if (name_taken(x.name, x.name_length, text, length, hash, x.hash)) return false;
    }
    for (int i = 0; i < r.watch_count + r.shown_count; ++i) {
        int n = 0;
        const char* m = ref_name(r, r.watch_index[i], n);
        if (name_taken(m, n, text, length, hash, fnv1a32(m, static_cast<std::size_t>(n)))) return false;
    }
    return true;
}

int add_tunable(Registry& r, const char* name, int length, std::uint32_t hash) {
    if (r.tunable_count >= r.max_tunables) {
        NNH_ASSERT(false, "live: more than %d tunables and actions; raise live::configure({.max_tunables})",
                   r.max_tunables);
        return -1;
    }
    int slot = 0;
    while ((r.entries[slot].flags & kUsed) != 0) ++slot;
    Entry& e = r.entries[slot];
    e = Entry{};
    e.flags = kUsed;
    e.hash = hash;
    e.generation = new_generation();
    e.name_length = static_cast<std::uint8_t>(length);
    std::memcpy(e.name, name, static_cast<std::size_t>(length));
    sorted_insert(r.tunable_index, r.tunable_count, static_cast<std::uint16_t>(slot), e.name, length,
                  [&](std::uint16_t s, int& n) {
                      n = r.entries[s].name_length;
                      return static_cast<const char*>(r.entries[s].name);
                  });
    ++r.tunable_count;
    return slot;
}

int add_watch(Registry& r, const char* name, int length, std::uint32_t hash) {
    if (r.watch_count >= r.max_watches) {
        NNH_ASSERT(false, "live: more than %d registered watches; raise live::configure({.max_watches})", r.max_watches);
        return -1;
    }
    int slot = 0;
    while ((r.watches[slot].flags & kUsed) != 0) ++slot;
    WatchEntry& w = r.watches[slot];
    w = WatchEntry{};
    w.flags = kUsed;
    w.hash = hash;
    w.generation = new_generation();
    w.name_length = static_cast<std::uint8_t>(length);
    std::memcpy(w.name, name, static_cast<std::size_t>(length));
    sorted_insert(r.watch_index, r.watch_count + r.shown_count, static_cast<std::uint16_t>(slot), w.name, length,
                  [&](std::uint16_t ref, int& n) { return ref_name(r, ref, n); });
    ++r.watch_count;
    return slot;
}

int find_or_add_shown(Registry& r, const char* name, int length, std::uint32_t hash, bool& added) {
    added = false;
    const int buckets = r.shown_buckets;
    if (buckets == 0) return -1;
    int b = static_cast<int>(hash & static_cast<std::uint32_t>(buckets - 1));
    for (int probe = 0; probe < buckets; ++probe, b = b + 1 == buckets ? 0 : b + 1) {
        const std::uint16_t v = r.shown_table[b];
        if (v == 0) break;
        const ShownEntry& s = r.shown[v - 1];
        if (s.hash == hash && compare_names(s.name, s.name_length, name, length) == 0) return v - 1;
    }
    if (r.shown_count >= r.max_shown) return -1;
    if (!name_usable(r, name, length)) return -1;
    int slot = 0;
    while ((r.shown[slot].flags & kUsed) != 0) ++slot;
    ShownEntry& s = r.shown[slot];
    s = ShownEntry{};
    s.flags = kUsed;
    s.hash = hash;
    s.name_length = static_cast<std::uint8_t>(length);
    std::memcpy(s.name, name, static_cast<std::size_t>(length));
    while (r.shown_table[b] != 0) b = b + 1 == buckets ? 0 : b + 1;
    r.shown_table[b] = static_cast<std::uint16_t>(slot + 1);
    sorted_insert(r.watch_index, r.watch_count + r.shown_count, static_cast<std::uint16_t>(kShownRef | slot), s.name,
                  length, [&](std::uint16_t ref, int& n) { return ref_name(r, ref, n); });
    ++r.shown_count;
    added = true;
    return slot;
}

void remove_tunable(Registry& r, int slot) {
    Entry& e = r.entries[slot];
    if ((e.flags & (kPending | kRunPending)) != 0) --r.pending_count;
    drop_follows(r, e.hash);
    sorted_erase(r.tunable_index, r.tunable_count, static_cast<std::uint16_t>(slot));
    --r.tunable_count;
    const std::uint32_t hash = e.hash;
    const bool seen = announced(r, e.flags);
    e = Entry{};
    if (seen && g_on_removed != nullptr) g_on_removed(hash);
}

void remove_watch(Registry& r, int slot) {
    WatchEntry& w = r.watches[slot];
    drop_follows(r, w.hash);
    sorted_erase(r.watch_index, r.watch_count + r.shown_count, static_cast<std::uint16_t>(slot));
    --r.watch_count;
    const std::uint32_t hash = w.hash;
    const bool seen = announced(r, w.flags);
    w = WatchEntry{};
    if (seen && g_on_removed != nullptr) g_on_removed(hash);
}

void remove_shown(Registry& r, int slot) {
    ShownEntry& s = r.shown[slot];
    drop_follows(r, s.hash);
    sorted_erase(r.watch_index, r.watch_count + r.shown_count, static_cast<std::uint16_t>(kShownRef | slot));
    --r.shown_count;
    // Rebuild the hash table without the slot (open addressing: deleting in place would break later probes).
    const int buckets = r.shown_buckets;
    for (int b = 0; b < buckets; ++b) r.shown_table[b] = 0;
    const std::uint32_t hash = s.hash;
    const bool seen = announced(r, s.flags);
    s = ShownEntry{};
    for (int i = 0; i < r.max_shown; ++i) {
        if ((r.shown[i].flags & kUsed) == 0) continue;
        int b = static_cast<int>(r.shown[i].hash & static_cast<std::uint32_t>(buckets - 1));
        while (r.shown_table[b] != 0) b = b + 1 == buckets ? 0 : b + 1;
        r.shown_table[b] = static_cast<std::uint16_t>(i + 1);
    }
    if (seen && g_on_removed != nullptr) g_on_removed(hash);
}

Entry* tunable_of(Registry& r, std::uint32_t id) {
    if (id == 0 || id_is_watch(id)) return nullptr;
    const int slot = id_slot(id);
    if (slot >= r.max_tunables) return nullptr;
    Entry& e = r.entries[slot];
    return (e.flags & kUsed) != 0 && e.generation == id_generation(id) ? &e : nullptr;
}

WatchEntry* watch_of(Registry& r, std::uint32_t id) {
    if (id == 0 || !id_is_watch(id)) return nullptr;
    const int slot = id_slot(id);
    if (slot >= r.max_watches) return nullptr;
    WatchEntry& w = r.watches[slot];
    return (w.flags & kUsed) != 0 && w.generation == id_generation(id) ? &w : nullptr;
}

int find_tunable(const Registry& r, std::uint32_t hash) {
    for (int i = 0; i < r.tunable_count; ++i) {
        if (r.entries[r.tunable_index[i]].hash == hash) return r.tunable_index[i];
    }
    return -1;
}

int find_watch(const Registry& r, std::uint32_t hash) {
    for (int i = 0; i < r.max_watches; ++i) {
        if ((r.watches[i].flags & kUsed) != 0 && r.watches[i].hash == hash) return i;
    }
    return -1;
}

int find_shown(const Registry& r, std::uint32_t hash) {
    for (int i = 0; i < r.max_shown; ++i) {
        if ((r.shown[i].flags & kUsed) != 0 && r.shown[i].hash == hash) return i;
    }
    return -1;
}

} // namespace detail::live
} // namespace nnh
