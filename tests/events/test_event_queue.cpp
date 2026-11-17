// Ring buffer of event records (events/event_queue.hpp).

#include <cstdint>
#include <cstring>

#include "events/event_queue.hpp"
#include "nnh/detail/type_id.hpp"
#include "test.hpp"

using nnh::detail::EventQueue;
using nnh::detail::EventRecord;
using nnh::detail::type_id;

namespace {

struct Small {
    int value;
};
struct Large {
    char bytes[100];
};

alignas(8) unsigned char g_buffer[4096];

int front_value(EventQueue& q) {
    EventRecord r;
    if (!q.front(r)) return -1;
    return static_cast<const Small*>(r.payload)->value;
}

} // namespace

NNH_TEST(queue_starts_empty) {
    EventQueue q;
    q.reset(g_buffer, sizeof g_buffer);
    EventRecord r;
    CHECK(q.empty());
    CHECK(!q.front(r));
}

NNH_TEST(events_come_out_in_publish_order) {
    EventQueue q;
    q.reset(g_buffer, sizeof g_buffer);
    for (int i = 1; i <= 3; ++i) {
        const Small e{i};
        CHECK(q.push(type_id<Small>(), &e, sizeof e, 0));
    }
    for (int i = 1; i <= 3; ++i) {
        CHECK(front_value(q) == i);
        q.pop();
    }
    CHECK(q.empty());
}

NNH_TEST(records_keep_type_size_depth_and_increasing_sequence) {
    EventQueue q;
    q.reset(g_buffer, sizeof g_buffer);
    const Small a{1};
    const Large b{};
    CHECK(q.push(type_id<Small>(), &a, sizeof a, 2));
    CHECK(q.push(type_id<Large>(), &b, sizeof b, 0));
    EventRecord r;
    CHECK(q.front(r));
    CHECK(r.type == type_id<Small>() && r.size == sizeof a && r.depth == 2);
    const std::uint32_t first = r.sequence;
    q.pop();
    CHECK(q.front(r));
    CHECK(r.type == type_id<Large>() && r.size == sizeof b);
    CHECK(r.sequence == first + 1);
    CHECK(q.next_sequence() == first + 2);
}

NNH_TEST(payloads_are_8_byte_aligned) {
    EventQueue q;
    q.reset(g_buffer, sizeof g_buffer);
    const char odd[3] = {1, 2, 3};
    const Small s{9};
    CHECK(q.push(type_id<char>(), odd, sizeof odd, 0));
    CHECK(q.push(type_id<Small>(), &s, sizeof s, 0));
    EventRecord r;
    for (int i = 0; i < 2; ++i) {
        CHECK(q.front(r));
        CHECK(reinterpret_cast<std::uintptr_t>(r.payload) % 8 == 0);
        q.pop();
    }
}

NNH_TEST(a_full_queue_refuses_pushes) {
    alignas(8) unsigned char tiny[256];
    EventQueue q;
    q.reset(tiny, sizeof tiny);
    const Large big{};
    int pushed = 0;
    while (q.push(type_id<Large>(), &big, sizeof big, 0)) ++pushed;
    CHECK(pushed >= 1 && pushed < 4);
    CHECK(!q.empty());
}

NNH_TEST(the_queue_wraps_around_many_times) {
    alignas(8) unsigned char ring[512];
    EventQueue q;
    q.reset(ring, sizeof ring);
    int expected = 0;
    int next = 0;
    for (int round = 0; round < 500; ++round) {
        // keep 2-3 events in flight so the write position laps the buffer repeatedly
        const Small e{next};
        if (q.push(type_id<Small>(), &e, sizeof e, 0)) ++next;
        const Large filler{};
        if (round % 3 == 0) q.push(type_id<Large>(), &filler, sizeof filler, 0);
        EventRecord r;
        while (q.front(r) && r.type == type_id<Large>()) q.pop();
        if (round % 2 == 1) {
            while (q.front(r)) {
                if (r.type == type_id<Small>()) {
                    CHECK(static_cast<const Small*>(r.payload)->value == expected);
                    ++expected;
                }
                q.pop();
            }
        }
    }
    CHECK(expected > 200);
}

NNH_TEST(a_record_that_wraps_to_the_start_is_read_after_the_older_ones) {
    // Records are a 16-byte header + payload rounded to 8: Small = 24 bytes, Mid = 40 bytes.
    struct Mid {
        int value;
        char pad[20];
    };
    alignas(8) unsigned char ring[128];
    EventQueue q;
    q.reset(ring, sizeof ring);
    for (int i = 0; i < 4; ++i) { // offsets 0, 24, 48, 72 -> tail at 96
        const Small e{i};
        CHECK(q.push(type_id<Small>(), &e, sizeof e, 0));
    }
    q.pop();
    q.pop(); // head at 48
    const Mid m{99, {}};
    CHECK(q.push(type_id<Mid>(), &m, sizeof m, 0)); // 32 bytes left at the end: wrap marker, record at 0
    CHECK(front_value(q) == 2);
    q.pop();
    CHECK(front_value(q) == 3);
    q.pop();
    EventRecord r;
    CHECK(q.front(r));
    CHECK(r.type == type_id<Mid>() && static_cast<const Mid*>(r.payload)->value == 99);
    q.pop();
    CHECK(q.empty());
}

NNH_TEST(pushing_while_reading_never_overwrites_the_front_record) {
    alignas(8) unsigned char ring[256];
    EventQueue q;
    q.reset(ring, sizeof ring);
    const Small first{42};
    CHECK(q.push(type_id<Small>(), &first, sizeof first, 0));
    EventRecord r;
    CHECK(q.front(r));
    const Small more{7};
    while (q.push(type_id<Small>(), &more, sizeof more, 1)) {
    }
    CHECK(static_cast<const Small*>(r.payload)->value == 42);
}

NNH_TEST(zero_sized_events_are_allowed) {
    struct Empty {};
    EventQueue q;
    q.reset(g_buffer, sizeof g_buffer);
    CHECK(q.push(type_id<Empty>(), nullptr, 0, 0));
    EventRecord r;
    CHECK(q.front(r) && r.size == 0);
}
