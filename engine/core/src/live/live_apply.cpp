// The application point (see live_apply.hpp).

#include "live/live_apply.hpp"

#include <cstring>

#include "live/live_names.hpp"
#include "live/live_registry.hpp"
#include "nnh/profile.hpp"
#include "remote/live_wire.hpp"

namespace nnh::detail::live {

void (*g_on_refused)(std::uint16_t seq, Refusal reason, std::uint32_t hash) = nullptr;

namespace {

void mark_pending(Registry& r, Entry& e, std::uint8_t flag) {
    if ((e.flags & (kPending | kRunPending)) == 0) ++r.pending_count;
    e.flags |= flag;
}

void clear_pending(Registry& r, Entry& e) {
    if ((e.flags & (kPending | kRunPending)) != 0) --r.pending_count;
    e.flags &= static_cast<std::uint8_t>(~(kPending | kRunPending));
}

/// Every tunable of the group whose path hashes to `group` (at any level).
template <typename Fn>
void each_in_group(Registry& r, std::uint32_t group, Fn fn) {
    for (int i = 0; i < r.tunable_count; ++i) {
        Entry& e = r.entries[r.tunable_index[i]];
        for (int level = 0; level < nnh::live::kMaxGroupLevels; ++level) {
            if (group_hash(e.name, e.name_length, level) == group) {
                fn(e);
                break;
            }
        }
    }
}

bool group_exists(Registry& r, std::uint32_t group) {
    bool found = false;
    each_in_group(r, group, [&](Entry&) { found = true; });
    return found;
}

void resolve(Registry& r, const RemoteOp& op) {
    using remote::LiveOp;
    using remote::ResetScope;
    if (op.op == static_cast<std::uint8_t>(LiveOp::Reset) && op.arg != static_cast<std::uint8_t>(ResetScope::One)) {
        if (op.arg == static_cast<std::uint8_t>(ResetScope::All)) {
            for (int i = 0; i < r.tunable_count; ++i) {
                Entry& e = r.entries[r.tunable_index[i]];
                if (e.kind != Kind::Action) set_pending(r, r.tunable_index[i], e.def);
            }
        } else if (group_exists(r, op.hash)) {
            each_in_group(r, op.hash, [&](Entry& e) {
                if (e.kind != Kind::Action) set_pending(r, static_cast<int>(&e - r.entries), e.def);
            });
        } else {
            count_refusal(r, op.seq, Refusal::NotDeclared, op.hash);
        }
        return;
    }
    const int slot = find_tunable(r, op.hash);
    if (slot < 0) {
        const bool watched = find_watch(r, op.hash) >= 0 || find_shown(r, op.hash) >= 0;
        count_refusal(r, op.seq, watched ? Refusal::WrongKind : Refusal::NotDeclared, op.hash);
        return;
    }
    Entry& e = r.entries[slot];
    const bool action = e.kind == Kind::Action;
    if (op.op == static_cast<std::uint8_t>(LiveOp::Action)) {
        if (action) request_run(r, slot);
        else count_refusal(r, op.seq, Refusal::WrongKind, op.hash);
        return;
    }
    if (action) {
        count_refusal(r, op.seq, Refusal::WrongKind, op.hash);
        return;
    }
    if (op.op == static_cast<std::uint8_t>(LiveOp::Reset)) {
        set_pending(r, slot, e.def);
        return;
    }
    const Refusal why = check_value(rule_of(e), op.bits);
    if (why != Refusal::None) count_refusal(r, op.seq, why, op.hash);
    else set_pending(r, slot, op.bits);
}

struct Due {
    std::uint16_t slot;
    std::uint16_t generation;
};

/// Up to `room` waiting entries in name order, their waiting flags cleared.
int collect(Registry& r, Due* due, int room, bool* run) {
    int n = 0;
    for (int i = 0; i < r.tunable_count && n < room; ++i) {
        Entry& e = r.entries[r.tunable_index[i]];
        if ((e.flags & (kPending | kRunPending)) == 0) continue;
        run[n] = (e.flags & kRunPending) != 0;
        due[n++] = {r.tunable_index[i], e.generation};
        clear_pending(r, e);
    }
    return n;
}

} // namespace

std::uint32_t read_bits(const Entry& e) {
    std::uint32_t bits = 0;
    switch (e.kind) {
    case Kind::Bool: return *static_cast<const bool*>(e.target) ? 1u : 0u;
    case Kind::Enum: {
        if (e.value_bytes == 1) return static_cast<std::uint8_t>(*static_cast<const std::uint8_t*>(e.target));
        if (e.value_bytes == 2) {
            std::uint16_t v = 0;
            std::memcpy(&v, e.target, 2);
            return v;
        }
        std::memcpy(&bits, e.target, 4);
        return bits;
    }
    case Kind::Color: {
        const auto* c = static_cast<const std::uint8_t*>(e.target);
        return pack_rgba(c[0], c[1], c[2], c[3]);
    }
    case Kind::Float:
    case Kind::Int32: std::memcpy(&bits, e.target, 4); return bits;
    default: return 0;
    }
}

void write_bits(Entry& e, std::uint32_t bits) {
    switch (e.kind) {
    case Kind::Bool: *static_cast<bool*>(e.target) = bits != 0; break;
    case Kind::Enum:
        if (e.value_bytes == 1) {
            *static_cast<std::uint8_t*>(e.target) = static_cast<std::uint8_t>(bits);
        } else if (e.value_bytes == 2) {
            const auto v = static_cast<std::uint16_t>(bits);
            std::memcpy(e.target, &v, 2);
        } else {
            std::memcpy(e.target, &bits, 4);
        }
        break;
    case Kind::Color: {
        auto* c = static_cast<std::uint8_t*>(e.target);
        for (int i = 0; i < 4; ++i) c[i] = channel(bits, i);
        break;
    }
    case Kind::Float:
    case Kind::Int32: std::memcpy(e.target, &bits, 4); break;
    default: break;
    }
}

ValueRule rule_of(const Entry& e) {
    ValueRule rule;
    rule.kind = e.kind;
    rule.min = e.min;
    rule.max = e.max;
    rule.count = e.enum_count;
    return rule;
}

void set_pending(Registry& r, int slot, std::uint32_t bits) {
    Entry& e = r.entries[slot];
    if (e.kind == Kind::Float && bits == 0x80000000u) bits = 0; // -0 is +0: it never differs from a +0 default
    e.pending = bits;
    mark_pending(r, e, kPending);
}

void request_run(Registry& r, int slot) { mark_pending(r, r.entries[slot], kRunPending); }

bool queue_op(Registry& r, const RemoteOp& op) {
    if (r.op_count >= kMaxOps) {
        count_refusal(r, op.seq, Refusal::QueueFull, op.hash);
        return false;
    }
    r.ops[r.op_count++] = op;
    return true;
}

void count_refusal(Registry& r, std::uint16_t seq, Refusal reason, std::uint32_t hash) {
    ++r.refused;
    ++r.refused_by_reason[static_cast<int>(reason) & 7];
    if (g_on_refused != nullptr) g_on_refused(seq, reason, hash);
}

void reset_now(Entry& e) {
    if (e.kind != Kind::Action) write_bits(e, e.def);
}

void frame_start(std::uint32_t frame) {
#if defined(NNH_FLAVOR_DEVELOPMENT)
    NNH_PROFILE_SCOPE("live start"); // the bench's hidden cost (Development only, like gfx submit)
#endif
    Registry* reg = g_registry;
    if (reg == nullptr || (reg->op_count == 0 && reg->pending_count == 0)) return;
    Registry& r = *reg;
    ++r.walks;
    for (int i = 0; i < r.op_count; ++i) resolve(r, r.ops[i]);
    r.op_count = 0;
    constexpr int kChunk = 32;
    Due due[kChunk];
    bool run[kChunk];
    for (int n = collect(r, due, kChunk, run); n > 0; n = collect(r, due, kChunk, run)) {
        for (int i = 0; i < n; ++i) {
            Entry& e = r.entries[due[i].slot];
            if ((e.flags & kUsed) == 0 || e.generation != due[i].generation) continue; // removed by a callback
            if (run[i]) {
                if (e.callback != nullptr) e.callback(e.context);
                continue;
            }
            if (e.kind == Kind::Action || read_bits(e) == e.pending) continue;
            write_bits(e, e.pending);
            e.changed_frame = frame;
            e.flags |= kChanged;
            if (e.callback != nullptr) e.callback(e.context);
        }
    }
}

} // namespace nnh::detail::live
