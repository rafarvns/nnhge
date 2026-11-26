// The live module's side of the remote (spec 030 FR-11, FR-14, FR-17, FR-18; plan "Remote protocol"): the catalogue
// in name order, LiveEntry/LiveRemoved while attached, Get and Follow answered by LiveValue records at the frame end,
// refusals as LiveRefused. Records are written in pieces from the registry (no buffer beyond a 72-byte scratch).
// Tests: tests/live/test_live_remote.cpp.

#include <cstring>

#include "live/live_apply.hpp"
#include "live/live_registry.hpp"
#include "live/live_watch.hpp"
#include "remote/live_wire.hpp"
#include "remote/remote_internal.hpp"

namespace nnh::detail::live {

extern void (*g_remote_op)(std::uint8_t, std::uint8_t, std::uint16_t, std::uint16_t, std::uint32_t, std::uint32_t,
                           std::uint32_t, bool);
extern void (*g_remote_detached)();
extern void (*g_follows_frame_end)(Registry&, std::uint32_t, bool);

namespace {

using remote::LiveOp;

bool g_records = false; // the PC asked for the catalogue in this attachment
std::uint32_t g_frame = 0;

void send(const std::uint8_t* bytes, std::size_t size) { remote::send_record(bytes, size); }

std::uint8_t entry_flags(const Entry& e) {
    std::uint8_t f = e.kind == Kind::Action ? remote::kEntryAction : remote::kEntryTunable;
    if (e.kind != Kind::Action && read_bits(e) != e.def) f |= remote::kEntryChanged;
    if ((e.flags & kPending) != 0) f |= remote::kEntryPending;
    return f;
}

void send_named(const remote::LiveEntryHeader& base, const char* name, int length, const char* const* enum_names,
                int enum_count) {
    remote::LiveEntryHeader h = base;
    h.name_length = static_cast<std::uint8_t>(length);
    h.enum_count = static_cast<std::uint8_t>(enum_count);
    std::uint32_t payload = remote::kLiveEntryFixed + static_cast<std::uint32_t>(length);
    for (int i = 0; i < enum_count; ++i) payload += 1 + static_cast<std::uint32_t>(std::strlen(enum_names[i]));
    std::uint8_t fixed[remote::kRecordHeaderSize + remote::kLiveEntryFixed];
    send(fixed, remote::encode_live_entry_fixed(h, payload, fixed));
    send(reinterpret_cast<const std::uint8_t*>(name), static_cast<std::size_t>(length));
    for (int i = 0; i < enum_count; ++i) {
        std::uint8_t piece[1 + remote::kMaxEnumNameBytes];
        const int n = static_cast<int>(std::strlen(enum_names[i]));
        piece[0] = static_cast<std::uint8_t>(n);
        std::memcpy(piece + 1, enum_names[i], static_cast<std::size_t>(n));
        send(piece, static_cast<std::size_t>(1 + n));
    }
}

void send_tunable(const Entry& e) {
    remote::LiveEntryHeader h;
    h.frame = g_frame;
    h.hash = e.hash;
    h.kind = e.kind;
    h.flags = entry_flags(e);
    h.value = e.kind == Kind::Action ? 0 : read_bits(e);
    h.def = e.def;
    h.min = e.min;
    h.max = e.max;
    h.step = e.step;
    send_named(h, e.name, e.name_length, e.enum_names, e.kind == Kind::Enum ? e.enum_count : 0);
}

void send_watch(const Registry& r, std::uint16_t ref) {
    remote::LiveEntryHeader h;
    h.frame = g_frame;
    if ((ref & kShownRef) != 0) {
        const ShownEntry& s = r.shown[ref & ~kShownRef];
        h.hash = s.hash;
        h.kind = s.kind == Kind::None ? Kind::Int32 : s.kind;
        h.flags = remote::kEntryWatch | remote::kEntryShown;
        send_named(h, s.name, s.name_length, nullptr, 0);
        return;
    }
    const WatchEntry& w = r.watches[ref];
    h.hash = w.hash;
    h.kind = w.kind;
    h.flags = remote::kEntryWatch;
    send_named(h, w.name, w.name_length, nullptr, 0);
}

void send_catalogue(const Registry& r) {
    for (int i = 0; i < r.tunable_count; ++i) send_tunable(r.entries[r.tunable_index[i]]);
    for (int i = 0; i < r.watch_count + r.shown_count; ++i) send_watch(r, r.watch_index[i]);
}

void on_added(int ref, bool watch) {
    if (!g_records || g_registry == nullptr) return;
    if (watch) send_watch(*g_registry, static_cast<std::uint16_t>(ref));
    else send_tunable(g_registry->entries[ref]);
}

void on_removed(std::uint32_t hash) {
    if (!g_records) return;
    std::uint8_t out[remote::kRecordHeaderSize + remote::kLiveRemovedPayload];
    send(out, remote::encode_live_removed(g_frame, hash, out));
}

void on_refused(std::uint16_t seq, Refusal reason, std::uint32_t hash) {
    std::uint8_t out[remote::kRecordHeaderSize + remote::kLiveRefusedPayload];
    send(out, remote::encode_live_refused(seq, reason, hash, out));
}

/// The value of whatever `hash` names into a record. False when nothing has that name.
bool value_of(Registry& r, std::uint32_t hash, remote::LiveValueRecord& v) {
    v.hash = hash;
    const int t = find_tunable(r, hash);
    if (t >= 0) {
        const Entry& e = r.entries[t];
        v.kind = e.kind;
        const std::uint32_t bits = read_bits(e);
        std::memcpy(v.value, &bits, 4);
        v.length = e.kind == Kind::Action ? 0 : 4;
        if ((e.flags & kPending) != 0) v.flags |= remote::kValuePending;
        if ((e.flags & kChanged) != 0 && e.changed_frame == g_frame) v.flags |= remote::kValueChangedThisFrame;
        return true;
    }
    const int w = find_watch(r, hash);
    const int s = w < 0 ? find_shown(r, hash) : -1;
    if (w < 0 && s < 0) return false;
    const WatchValue wv = w >= 0 ? read_watch(r.watches[w]) : read_shown(r.shown[s]);
    v.kind = wv.kind;
    v.length = wv.length;
    std::memcpy(v.value, wv.bytes, sizeof v.value);
    if (s >= 0 && r.shown[s].last_frame != g_frame) v.flags |= remote::kValueStale;
    return true;
}

void follows_frame_end(Registry& r, std::uint32_t frame, bool paused) {
    g_frame = frame;
    int kept = 0;
    for (int i = 0; i < r.follow_count; ++i) {
        Follow f = r.follows[i];
        bool keep = true;
        if (frame >= f.next_frame) {
            remote::LiveValueRecord v;
            v.frame = frame;
            if (paused) v.flags |= remote::kValuePaused;
            if (value_of(r, f.hash, v)) {
                std::uint8_t out[remote::kRecordHeaderSize + remote::kLiveValueMax];
                send(out, remote::encode_live_value(v, out));
            }
            f.next_frame = frame + f.every;
            if (f.once != 0 || (f.left != 0 && --f.left == 0)) keep = false;
        }
        if (keep) r.follows[kept++] = f;
    }
    r.follow_count = kept;
}

void add_follow(Registry& r, std::uint16_t seq, std::uint32_t hash, std::uint8_t every, std::uint16_t count, bool once,
                std::uint32_t frame) {
    if (find_tunable(r, hash) < 0 && find_watch(r, hash) < 0 && find_shown(r, hash) < 0) {
        count_refusal(r, seq, Refusal::NotDeclared, hash);
        return;
    }
    if (r.follow_count >= kMaxFollows) {
        count_refusal(r, seq, Refusal::FollowsFull, hash);
        return;
    }
    Follow& f = r.follows[r.follow_count++];
    f.hash = hash;
    f.every = every == 0 ? 1 : every;
    f.left = count;
    f.once = once ? 1 : 0;
    f.next_frame = frame;
}

void unfollow(Registry& r, std::uint32_t hash) {
    int kept = 0;
    for (int i = 0; i < r.follow_count; ++i) {
        if (hash != 0 && r.follows[i].hash != hash) r.follows[kept++] = r.follows[i];
    }
    r.follow_count = kept;
}

void handle_op(std::uint8_t op, std::uint8_t arg, std::uint16_t count, std::uint16_t seq, std::uint32_t hash,
               std::uint32_t bits, std::uint32_t frame, bool paused) {
    Registry& r = registry();
    g_frame = paused ? frame - 1 : frame;
    switch (static_cast<LiveOp>(op)) {
    case LiveOp::Catalogue:
        g_records = true;
        send_catalogue(r);
        return;
    case LiveOp::Get: add_follow(r, seq, hash, 1, 1, true, g_frame); return;
    case LiveOp::Follow: add_follow(r, seq, hash, arg, count, false, g_frame); return;
    case LiveOp::Unfollow: unfollow(r, hash); return;
    case LiveOp::Dump: r.dump_requested = true; return;
    default: break;
    }
    RemoteOp q;
    q.op = op;
    q.arg = arg;
    q.seq = seq;
    q.hash = hash;
    q.bits = bits;
    queue_op(r, q);
}

void detached() {
    g_records = false;
    if (g_registry != nullptr) g_registry->follow_count = 0;
}

} // namespace

/// Installs the remote side (live_ops.cpp's install_ops()).
void install_remote() {
    g_remote_op = handle_op;
    g_remote_detached = detached;
    g_follows_frame_end = follows_frame_end;
    g_on_added = on_added;
    g_on_removed = on_removed;
    g_on_refused = on_refused;
    g_records = false;
}

void uninstall_remote() {
    g_on_added = nullptr;
    g_on_removed = nullptr;
    g_on_refused = nullptr;
    g_records = false;
}

} // namespace nnh::detail::live
