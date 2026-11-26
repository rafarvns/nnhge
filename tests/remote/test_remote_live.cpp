// Live values over the session (spec 030 T5.1, T5.2, FR-11, FR-16, FR-17, FR-18): a set applied at the frame it names,
// the catalogue in name order and its end, LiveEntry while attached, Get and Follow answered by LiveValue, refusals as
// LiveRefused, no live record before a catalogue request, a game without the module, Pause / step / resume with
// LiveState records, values while paused flagged.

#include <cstring>

#include "live/live_kinds.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/live.hpp"
#include "nnh/platform/system.hpp"
#include "remote/live_wire.hpp"
#include "remote_fixture.hpp"

using namespace remote_test;
namespace live = nnh::live;
using nnh::detail::live::float_bits;

namespace {

void restart_attached() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init({}));
    fake::remote_connect();
    send(hello());
    CHECK(nnh::frame()); // frame 1: attached
}

wire::Message stamped(wire::Message m, std::uint16_t seq, std::uint32_t frame) {
    m.seq = seq;
    m.frame = frame;
    return m;
}

std::uint32_t hash(const char* name) { return nnh::detail::fnv1a32(name); }

} // namespace

NNH_TEST(remote_live_set_lands_at_the_frame_it_names) {
    if constexpr (!live::kEnabled) return;
    restart_attached();
    float jump = 4.5f;
    live::Handle h = live::tune("player/jump", &jump, {.min = 0, .max = 10, .step = 0.25f});
    send(stamped(wire::live_set_message(hash("player/jump"), float_bits(5.25f)), 2, 4));
    CHECK(nnh::frame()); // 2
    CHECK(nnh::frame()); // 3
    CHECK(jump == 4.5f);
    CHECK(nnh::frame()); // 4: applied at its start
    CHECK(jump == 5.25f && h.changed());
}

NNH_TEST(remote_live_no_record_before_a_catalogue_request) {
    if constexpr (!live::kEnabled) return;
    restart_attached();
    float a = 0;
    live::Handle h = live::tune("a", &a, {.min = 0, .max = 1, .step = 1});
    CHECK(nnh::frame());
    CHECK(sent().count_of(wire::RecordType::LiveEntry) == 0);
}

NNH_TEST(remote_live_catalogue_in_name_order_then_its_end) {
    if constexpr (!live::kEnabled) return;
    restart_attached();
    float b = 1, a = 2;
    int n = 3;
    live::Handle hb = live::tune("b/x", &b, {.min = 0, .max = 4, .step = 1});
    live::Handle ha = live::tune("a/y", &a, {.min = 0, .max = 4, .step = 1});
    live::Handle hw = live::watch("w/n", &n);
    fake::remote_clear_sent();
    send(stamped(wire::live_message(wire::LiveOp::Catalogue, 0, 0, 0), 2, 0));
    CHECK(nnh::frame());
    const Sent s = sent();
    CHECK(s.whole);
    CHECK(s.count_of(wire::RecordType::LiveEntry) == 3);
    wire::LiveEntryView v;
    CHECK(wire::decode_live_entry(s.find(wire::RecordType::LiveEntry, 0)->payload,
                                  s.find(wire::RecordType::LiveEntry, 0)->length, v));
    CHECK(v.header.hash == hash("a/y") && v.header.def == float_bits(2.0f));
    CHECK(wire::decode_live_entry(s.find(wire::RecordType::LiveEntry, 2)->payload,
                                  s.find(wire::RecordType::LiveEntry, 2)->length, v));
    CHECK(v.header.hash == hash("w/n") && (v.header.flags & wire::kEntryWatch) != 0);
    const Record* state = s.find(wire::RecordType::LiveState);
    CHECK(state != nullptr && (state->payload[5] & wire::kStateCatalogueComplete) != 0);
    // registered after the catalogue: sent at once
    fake::remote_clear_sent();
    float c = 0;
    live::Handle hc = live::tune("c", &c, {.min = 0, .max = 1, .step = 1});
    CHECK(sent().count_of(wire::RecordType::LiveEntry) == 1);
    hc = live::Handle{};
    CHECK(sent().count_of(wire::RecordType::LiveRemoved) == 1);
}

NNH_TEST(remote_live_get_and_follow_answer_with_values) {
    if constexpr (!live::kEnabled) return;
    restart_attached();
    float a = 1.5f;
    live::Handle h = live::tune("a", &a, {.min = 0, .max = 4, .step = 0.5f});
    fake::remote_clear_sent();
    send(stamped(wire::live_message(wire::LiveOp::Get, 0, 0, hash("a")), 2, 0));
    send(stamped(wire::live_message(wire::LiveOp::Follow, 2, 3, hash("a")), 3, 0));
    for (int i = 0; i < 8; ++i) CHECK(nnh::frame());
    const Sent s = sent();
    CHECK(s.count_of(wire::RecordType::LiveValue) == 1 + 3); // the get, then three follows every 2 frames
    wire::LiveValueRecord v;
    CHECK(wire::decode_live_value(s.find(wire::RecordType::LiveValue)->payload,
                                  s.find(wire::RecordType::LiveValue)->length, v));
    std::uint32_t bits = 0;
    std::memcpy(&bits, v.value, 4);
    CHECK(v.hash == hash("a") && bits == float_bits(1.5f));
}

NNH_TEST(remote_live_refusals_are_records) {
    if constexpr (!live::kEnabled) return;
    restart_attached();
    float a = 0;
    live::Handle h = live::tune("a", &a, {.min = 0, .max = 1, .step = 1});
    fake::remote_clear_sent();
    send(stamped(wire::live_set_message(hash("nope"), 0), 7, 0));
    send(stamped(wire::live_set_message(hash("a"), float_bits(3.0f)), 8, 0));
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    const Sent s = sent();
    CHECK(s.count_of(wire::RecordType::LiveRefused) == 2);
    wire::LiveRefusedRecord r;
    CHECK(wire::decode_live_refused(s.find(wire::RecordType::LiveRefused, 1)->payload, 8, r));
    CHECK(r.seq == 8 && r.reason == nnh::detail::live::Refusal::OutOfRange);
    CHECK(a == 0);
}

NNH_TEST(remote_live_a_game_without_live_values_refuses_and_lists_nothing) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart_attached();
    fake::remote_clear_sent();
    send(stamped(wire::live_message(wire::LiveOp::Catalogue, 0, 0, 0), 2, 0));
    send(stamped(wire::live_set_message(hash("a"), 0), 3, 0));
    CHECK(nnh::frame());
    const Sent s = sent();
    CHECK(s.count_of(wire::RecordType::LiveEntry) == 0);
    CHECK(s.count_of(wire::RecordType::LiveState) == 1);
    wire::LiveRefusedRecord r;
    CHECK(s.find(wire::RecordType::LiveRefused) != nullptr);
    CHECK(wire::decode_live_refused(s.find(wire::RecordType::LiveRefused)->payload, 8, r));
    CHECK(r.reason == nnh::detail::live::Refusal::NoLiveValues);
}

NNH_TEST(remote_pause_step_and_resume_by_messages) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart_attached();
    const std::uint64_t before = nnh::frame_count();
    const std::uint32_t next = static_cast<std::uint32_t>(before) + 1;
    send(stamped(wire::pause_message(wire::PauseOp::Pause, 0), 2, next));
    send(stamped(wire::pause_message(wire::PauseOp::Step, 3), 3, next));
    // the console pauses at `next`, applies the step on a paused tick (both messages are due there), runs 3 frames
    for (int i = 0; i < 3; ++i) CHECK(nnh::frame());
    CHECK(nnh::frame_count() == before + 3);
    send(stamped(wire::pause_message(wire::PauseOp::Resume, 0), 4, next + 3));
    CHECK(nnh::frame());
    CHECK(nnh::frame_count() == before + 4);
    CHECK(sent().count_of(wire::RecordType::LiveState) >= 3);
}

NNH_TEST(remote_live_values_while_paused_are_flagged) {
    if constexpr (!live::kEnabled) return;
    restart_attached();
    float a = 2;
    live::Handle h = live::tune("a", &a, {.min = 0, .max = 4, .step = 1});
    const std::uint32_t next = static_cast<std::uint32_t>(nnh::frame_count()) + 1;
    send(stamped(wire::pause_message(wire::PauseOp::Pause, 0), 2, next));
    send(stamped(wire::live_message(wire::LiveOp::Get, 0, 0, hash("a")), 3, next));
    fake::queue_dev_requests({0, 0, nnh::platform::kDevTogglePause}); // two paused ticks, then the simulator's Pause
    fake::remote_clear_sent();
    CHECK(nnh::frame());
    const Sent s = sent();
    const Record* value = s.find(wire::RecordType::LiveValue);
    CHECK(value != nullptr);
    wire::LiveValueRecord v;
    CHECK(value != nullptr && wire::decode_live_value(value->payload, value->length, v));
    CHECK((v.flags & wire::kValuePaused) != 0);
}
