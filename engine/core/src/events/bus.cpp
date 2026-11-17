// The event bus: subscribe, publish, dispatch (nnh/events.hpp, docs/architecture/events.md).
//
// One queue and one subscriber table, allocated at init(). Dispatch runs once per frame and delivers
// queued events in FIFO order, including events handlers publish meanwhile (with a depth limit).

#include "events/event_queue.hpp"
#include "events/events_internal.hpp"
#include "events/subscriber_table.hpp"
#include "log/engine_channels.hpp"
#include "nnh/assert.hpp"
#include "nnh/events.hpp"
#include "nnh/memory.hpp"

namespace nnh {

NNH_MEM_TAG(events);

/// The only way to build a non-empty Subscription (its constructor is private).
struct SubscriptionAccess {
    static Subscription make(int slot, std::uint32_t generation) { return Subscription(slot, generation); }
};

namespace detail {
namespace {

constexpr std::size_t kMinQueueBytes = 1024;
constexpr int kMinSubscribers = 8;

EventQueue g_queue;
SubscriberTable g_table;
void* g_queue_memory = nullptr;
SubscriberSlot* g_slots = nullptr;
bool g_ready = false;
bool g_dispatching = false;
std::uint16_t g_depth = 0;          // depth of the event being delivered
std::uint32_t g_next_generation = 1; // keeps handles from a previous init() stale
int g_dropped = 0;
bool g_warned = false; // one warning per dispatch, so a flood doesn't flood the log

void free_memory() {
    mem::free(g_queue_memory);
    mem::free(g_slots);
    g_queue_memory = nullptr;
    g_slots = nullptr;
}

void warn_dropped(const char* reason) {
    ++g_dropped;
    if (g_warned) return;
    g_warned = true;
    NNH_LOG_WARN(events, "event dropped: %s", reason);
}

} // namespace

bool events_init(const EventsConfig& config) {
    std::size_t queue_bytes = config.queue_bytes;
    int subscribers = config.max_subscribers;
    if (queue_bytes < kMinQueueBytes || subscribers < kMinSubscribers) {
        NNH_LOG_WARN(events, "AppConfig::events below the minimum (%zu bytes, %d subscribers): raised", kMinQueueBytes,
                     kMinSubscribers);
        queue_bytes = queue_bytes < kMinQueueBytes ? kMinQueueBytes : queue_bytes;
        subscribers = subscribers < kMinSubscribers ? kMinSubscribers : subscribers;
    }
    g_queue_memory = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(events), queue_bytes);
    g_slots = static_cast<SubscriberSlot*>(mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(events),
                                                      sizeof(SubscriberSlot) * static_cast<std::size_t>(subscribers),
                                                      alignof(SubscriberSlot)));
    if (g_queue_memory == nullptr || g_slots == nullptr) {
        free_memory();
        NNH_LOG_ERROR(events, "out of memory for the event bus (%zu bytes queue, %d subscribers)", queue_bytes,
                      subscribers);
        return false;
    }
    g_queue.reset(g_queue_memory, queue_bytes);
    g_table.reset(g_slots, subscribers, g_next_generation);
    g_ready = true;
    g_dispatching = false;
    g_depth = 0;
    g_dropped = 0;
    g_warned = false;
    return true;
}

void events_shutdown() {
    if (!g_ready) return;
    NNH_VERIFY(!g_dispatching, "nnh::shutdown() called from an event handler");
    g_table.invalidate_all();
    g_next_generation = g_table.next_generation();
    g_table.reset(nullptr, 0);
    free_memory();
    g_ready = false;
}

void events_dispatch() {
    if (!g_ready) return;
    g_warned = false;
    g_dispatching = true;
    EventRecord record;
    while (g_queue.front(record)) {
        g_depth = record.depth;
        g_table.for_each_matching(record.type, record.sequence, [&](SubscriberSlot& slot) {
            // Run a copy: the handler may unsubscribe (clearing its slot) while it runs. Copy it back
            // afterwards so mutable lambdas keep their state.
            const std::uint32_t generation = slot.generation;
            EventHandler handler = slot.handler;
            handler(record.payload);
            if (slot.active && slot.generation == generation) slot.handler = handler;
        });
        g_queue.pop();
    }
    g_dispatching = false;
    g_depth = 0;
}

Subscription events_subscribe(TypeId type, EventHandler handler) {
    NNH_ASSERT(g_ready, "nnh::events::subscribe() before nnh::init() or after nnh::shutdown()");
    if (!g_ready) return {};
    // During dispatch: only later publishes. Otherwise: everything still queued.
    EventRecord front;
    const std::uint32_t since =
        !g_dispatching && g_queue.front(front) ? front.sequence : g_queue.next_sequence();
    std::uint32_t generation = 0;
    const int slot = g_table.add(type, handler, since, generation);
    if (slot < 0) {
        NNH_LOG_WARN(events, "subscriber table full (%d): raise AppConfig::events.max_subscribers",
                     g_table.capacity());
        NNH_ASSERT(false, "event subscriber table full (%d subscribers)", g_table.capacity());
        return {};
    }
    return SubscriptionAccess::make(slot, generation);
}

void events_publish(TypeId type, const void* data, std::size_t size) {
    NNH_ASSERT(g_ready, "nnh::events::publish() before nnh::init() or after nnh::shutdown()");
    if (!g_ready) return;
    const int depth = g_dispatching ? g_depth + 1 : 0;
    if (depth > events::kMaxDepth) {
        warn_dropped("depth limit reached (an event loop: handlers publishing events that lead back to them)");
        return;
    }
    if (!g_queue.push(type, data, size, static_cast<std::uint16_t>(depth))) {
        warn_dropped("queue full: raise AppConfig::events.queue_bytes");
        NNH_ASSERT(false, "event queue full (%zu bytes)", g_queue.capacity());
    }
}

void events_unsubscribe(int slot, std::uint32_t generation) {
    if (g_ready) g_table.remove(slot, generation);
}

bool events_is_subscribed(int slot, std::uint32_t generation) { return g_ready && g_table.is_active(slot, generation); }

} // namespace detail

events::Stats events::stats() {
    Stats s;
    if (!detail::g_ready) return s;
    s.queue_bytes = detail::g_queue.capacity();
    s.max_subscribers = detail::g_table.capacity();
    s.subscribers = detail::g_table.active_count();
    s.dropped_events = detail::g_dropped;
    return s;
}

} // namespace nnh
