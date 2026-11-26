// The application point (spec 030 T2.3, FR-3, FR-4, FR-5, Q6, Q9): panel and remote edits land at the start of the
// next game frame (after nnh::frame() returns), changed() is true on exactly that frame, same bits change nothing,
// callbacks run in name order and survive registrations and removals, a set for a tunable a scene registers in that
// frame's on_enter() lands, a full queue refuses, actions run once.

#include "live/live_apply.hpp"
#include "live/live_registry.hpp"
#include "live_fixture.hpp"
#include "nnh/scene.hpp"
#include "remote/live_wire.hpp"

using namespace nnh::detail::live;
using live_test::frames;
using live_test::restart;
namespace live = nnh::live;
namespace remote = nnh::detail::remote;

namespace {

RemoteOp set_op(const char* name, std::uint32_t bits) {
    RemoteOp op;
    op.op = kOpSet;
    op.hash = nnh::detail::fnv1a32(name);
    op.bits = bits;
    return op;
}

RemoteOp live_op(remote::LiveOp which, std::uint8_t arg, const char* name) {
    RemoteOp op;
    op.op = static_cast<std::uint8_t>(which);
    op.arg = arg;
    op.hash = name == nullptr ? 0 : nnh::detail::fnv1a32(name);
    return op;
}

char g_order[8];
int g_order_n = 0;
void note_a(void*) { g_order[g_order_n++] = 'a'; }
void note_b(void*) { g_order[g_order_n++] = 'b'; }

live::Handle* g_victim = nullptr;
void destroy_victim(void*) { *g_victim = live::Handle{}; }

} // namespace

NNH_TEST(live_apply_remote_set_lands_at_the_next_frame_start) {
    if constexpr (!live::kEnabled) return;
    restart();
    float jump = 4.5f;
    live::Handle h = live::tune("player/jump", &jump, {.min = 0, .max = 10, .step = 0.25f});
    frames(1);
    CHECK(queue_op(*g_registry, set_op("player/jump", float_bits(5.25f))));
    CHECK(jump == 4.5f); // nothing until the application point
    frames(1);
    CHECK(jump == 5.25f);
    CHECK(h.changed());
    frames(1);
    CHECK(!h.changed());
    CHECK(jump == 5.25f);
}

NNH_TEST(live_apply_panel_edit_lands_at_the_next_frame_start) {
    if constexpr (!live::kEnabled) return;
    restart();
    std::int32_t n = 6;
    live::Handle h = live::tune("spawn/count", &n, {.min = 0, .max = 32, .step = 1});
    set_pending(*g_registry, id_slot(h.id()), 9);
    CHECK(n == 6 && !h.changed());
    frames(1);
    CHECK(n == 9 && h.changed());
}

NNH_TEST(live_apply_same_bits_change_nothing) {
    if constexpr (!live::kEnabled) return;
    restart();
    float v = 1;
    int calls = 0;
    live::Handle h = live::tune("a/v", &v, {.min = 0, .max = 2, .step = 1},
                                {[](void* c) { ++*static_cast<int*>(c); }, &calls});
    queue_op(*g_registry, set_op("a/v", float_bits(1.0f)));
    frames(1);
    CHECK(!h.changed() && calls == 0);
    queue_op(*g_registry, set_op("a/v", float_bits(2.0f)));
    frames(1);
    CHECK(h.changed() && calls == 1);
}

NNH_TEST(live_apply_latest_edit_wins) {
    if constexpr (!live::kEnabled) return;
    restart();
    float v = 0;
    live::Handle h = live::tune("a/v", &v, {.min = 0, .max = 9, .step = 1});
    set_pending(*g_registry, id_slot(h.id()), float_bits(3.0f));
    queue_op(*g_registry, set_op("a/v", float_bits(7.0f)));
    set_pending(*g_registry, id_slot(h.id()), float_bits(5.0f));
    frames(1);
    CHECK(v == 7.0f); // remote operations are resolved at the point, over the panel's earlier pending values
    CHECK(g_registry->pending_count == 0);
}

NNH_TEST(live_apply_callbacks_run_in_name_order) {
    if constexpr (!live::kEnabled) return;
    restart();
    float x = 0, y = 0;
    live::Handle hb = live::tune("b/x", &x, {.min = 0, .max = 9, .step = 1}, {note_b, nullptr});
    live::Handle ha = live::tune("a/y", &y, {.min = 0, .max = 9, .step = 1}, {note_a, nullptr});
    g_order_n = 0;
    set_pending(*g_registry, id_slot(hb.id()), float_bits(1.0f));
    set_pending(*g_registry, id_slot(ha.id()), float_bits(1.0f));
    frames(1);
    CHECK(g_order_n == 2 && g_order[0] == 'a' && g_order[1] == 'b');
}

NNH_TEST(live_apply_a_callback_may_destroy_a_handle_waiting_after_it) {
    if constexpr (!live::kEnabled) return;
    restart();
    float x = 0, y = 0;
    live::Handle hb = live::tune("b/x", &x, {.min = 0, .max = 9, .step = 1}, {note_b, nullptr});
    live::Handle ha = live::tune("a/y", &y, {.min = 0, .max = 9, .step = 1}, {destroy_victim, nullptr});
    g_victim = &hb;
    g_order_n = 0;
    set_pending(*g_registry, id_slot(hb.id()), float_bits(4.0f));
    set_pending(*g_registry, id_slot(ha.id()), float_bits(1.0f));
    frames(1);
    CHECK(y == 1.0f);
    CHECK(x == 0.0f && g_order_n == 0); // removed before its turn: neither written nor called
    CHECK(!hb.valid());
}

namespace {
float g_late = 1;
live::Handle g_late_handle;
struct LateScene : nnh::Scene {
    void on_enter() override { g_late_handle = live::tune("late/v", &g_late, {.min = 0, .max = 9, .step = 1}); }
    void on_exit() override { g_late_handle = live::Handle{}; }
};
} // namespace

NNH_TEST(live_apply_a_set_lands_on_a_tunable_registered_in_that_frames_on_enter) {
    if constexpr (!live::kEnabled) return;
    restart();
    float other = 0;
    live::Handle h = live::tune("x/other", &other, {.min = 0, .max = 1, .step = 1}); // the registry exists
    static LateScene scene;
    nnh::scenes::push(scene); // applied at the start of the next frame
    queue_op(*g_registry, set_op("late/v", float_bits(8.0f)));
    frames(1);
    CHECK(g_late_handle.valid());
    CHECK(g_late == 8.0f && g_late_handle.changed());
    CHECK(g_registry->refused == 0);
    nnh::scenes::pop();
    frames(1);
}

NNH_TEST(live_apply_refuses_unknown_names_kinds_and_values) {
    if constexpr (!live::kEnabled) return;
    restart();
    float v = 0;
    nnh::Vec2 p{};
    live::Handle h = live::tune("a/v", &v, {.min = 0, .max = 9, .step = 1});
    live::Handle w = live::watch("a/p", &p);
    live::Handle a = live::action("a/go", [](void*) {});
    queue_op(*g_registry, set_op("a/nope", 0));
    queue_op(*g_registry, set_op("a/p", 0));
    queue_op(*g_registry, set_op("a/go", 0));
    queue_op(*g_registry, set_op("a/v", float_bits(10.0f)));
    queue_op(*g_registry, set_op("a/v", 0x7FC00000u));
    frames(1);
    CHECK(v == 0);
    CHECK(g_registry->refused == 5);
    CHECK(g_registry->refused_by_reason[static_cast<int>(Refusal::NotDeclared)] == 1);
    CHECK(g_registry->refused_by_reason[static_cast<int>(Refusal::WrongKind)] == 2);
    CHECK(g_registry->refused_by_reason[static_cast<int>(Refusal::OutOfRange)] == 1);
    CHECK(g_registry->refused_by_reason[static_cast<int>(Refusal::NotFinite)] == 1);
}

NNH_TEST(live_apply_a_full_queue_refuses) {
    if constexpr (!live::kEnabled) return;
    restart();
    float v = 0;
    live::Handle h = live::tune("a/v", &v, {.min = 0, .max = 99, .step = 1});
    for (int i = 0; i < kMaxOps; ++i) CHECK(queue_op(*g_registry, set_op("a/v", float_bits(1.0f))));
    CHECK(!queue_op(*g_registry, set_op("a/v", float_bits(2.0f))));
    CHECK(g_registry->refused_by_reason[static_cast<int>(Refusal::QueueFull)] == 1);
    frames(1);
    CHECK(v == 1.0f);
}

NNH_TEST(live_apply_remote_resets_go_through_the_application_point) {
    if constexpr (!live::kEnabled) return;
    restart();
    float a = 1, b = 2, c = 3;
    live::Handle ha = live::tune("p/a", &a, {.min = 0, .max = 9, .step = 1});
    live::Handle hb = live::tune("p/q/b", &b, {.min = 0, .max = 9, .step = 1});
    live::Handle hc = live::tune("c", &c, {.min = 0, .max = 9, .step = 1});
    a = 5;
    b = 6;
    c = 7;
    queue_op(*g_registry, live_op(remote::LiveOp::Reset, 1, "p")); // the group, nested groups included
    frames(1);
    CHECK(a == 1 && b == 2 && c == 7 && ha.changed() && hb.changed() && !hc.changed());
    a = 5;
    queue_op(*g_registry, live_op(remote::LiveOp::Reset, 0, "p/a"));
    frames(1);
    CHECK(a == 1 && ha.changed());
    a = 5;
    c = 8;
    queue_op(*g_registry, live_op(remote::LiveOp::Reset, 2, nullptr));
    frames(1);
    CHECK(a == 1 && c == 3 && hc.changed());
    queue_op(*g_registry, live_op(remote::LiveOp::Reset, 1, "nogroup"));
    frames(1);
    CHECK(g_registry->refused_by_reason[static_cast<int>(Refusal::NotDeclared)] == 1);
}

NNH_TEST(live_apply_actions_run_once_per_request_at_the_point) {
    if constexpr (!live::kEnabled) return;
    restart();
    int runs = 0;
    live::Handle h = live::action("player/respawn", [](void* c) { ++*static_cast<int*>(c); }, &runs);
    request_run(*g_registry, id_slot(h.id()));
    request_run(*g_registry, id_slot(h.id()));
    CHECK(runs == 0);
    frames(1);
    CHECK(runs == 1);
    queue_op(*g_registry, live_op(remote::LiveOp::Action, 0, "player/respawn"));
    frames(1);
    CHECK(runs == 2);
    frames(1);
    CHECK(runs == 2);
}

NNH_TEST(live_apply_nothing_is_walked_while_nothing_waits) {
    if constexpr (!live::kEnabled) return;
    restart();
    float v = 0;
    live::Handle h = live::tune("a/v", &v, {.min = 0, .max = 9, .step = 1});
    const std::uint32_t walks = g_registry->walks;
    frames(100);
    CHECK(g_registry->walks == walks);
    set_pending(*g_registry, id_slot(h.id()), float_bits(1.0f));
    frames(1);
    CHECK(g_registry->walks == walks + 1);
}
