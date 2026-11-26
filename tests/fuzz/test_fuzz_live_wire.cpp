// The remote fuzz target's corpus with spec 030's messages (T1.5): a session of LiveSet, Live and Pause messages,
// mutated 1-4 bytes at a time and sometimes cut; every decoded window must encode back to the same bytes, and the
// reader, schedule and input merge must stay within their bounds (remote_target's own checks).

#include <cstdint>
#include <cstring>

#include "fuzz_targets.hpp"
#include "remote/live_wire.hpp"
#include "remote/remote_wire.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;

namespace {

std::uint32_t next(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

} // namespace

NNH_TEST(remote_target_survives_mutated_live_messages) {
    Message messages[9];
    messages[0].type = MessageType::Hello;
    messages[0].version = kVersion;
    messages[1] = live_message(LiveOp::Catalogue, 0, 0, 0);
    messages[2] = live_set_message(0x7AD1C1B3u, 0x40A80000u);
    messages[3] = live_message(LiveOp::Follow, 2, 30, 0x11u);
    messages[4] = pause_message(PauseOp::Pause, 0);
    messages[5] = pause_message(PauseOp::Step, 3);
    messages[6] = live_message(LiveOp::Reset, 2, 0, 0);
    messages[7] = pause_message(PauseOp::Resume, 0);
    messages[8] = live_message(LiveOp::Dump, 0, 0, 0);
    std::uint8_t seed[9 * (1 + kMessageSize)];
    std::size_t size = 0;
    std::uint32_t frame = 2;
    for (Message& m : messages) {
        if (m.type != MessageType::Hello) m.frame = frame++;
        seed[size++] = static_cast<std::uint8_t>(kMessageSize | (1u << 6));
        encode_message(m, seed + size);
        size += kMessageSize;
    }
    std::uint32_t state = 3030;
    std::uint8_t mutant[sizeof seed];
    int decoded = 0;
    for (int i = 0; i < 20000; ++i) {
        std::memcpy(mutant, seed, size);
        const int changes = 1 + static_cast<int>(next(state) % 4);
        for (int c = 0; c < changes; ++c) mutant[next(state) % size] = static_cast<std::uint8_t>(next(state) >> 24);
        const std::size_t n = next(state) % 8 == 0 ? next(state) % size : size;
        nnh::fuzz::remote_target(mutant, n);
        Message m;
        if (decode_message(mutant + 1 + 2 * (1 + kMessageSize), m) == Reject::None) ++decoded;
    }
    CHECK(decoded > 0); // some mutants still decode: the deep path runs
}
