// Mini-fuzz of the live values' remote target (spec 030 T5.3, AC-5): a valid session (Hello, catalogue, sets of every
// kind, follows, resets, an action, a pause with steps and a resume, a dump), mutated 1-4 bytes at a time and sometimes
// cut. The target aborts on a broken invariant.

#include <cstdint>
#include <cstring>
#include <vector>

#include "live/live_kinds.hpp"
#include "nnh/live.hpp"
#include "remote/live_wire.hpp"
#include "remote/remote_wire.hpp"
#include "test.hpp"

namespace nnh::fuzz {
int live_remote_target(const std::uint8_t* data, std::size_t size);
}

using namespace nnh::detail::remote;

namespace {

std::uint32_t next(std::uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

void add(std::vector<std::uint8_t>& out, Message m, std::uint16_t seq, std::uint32_t frame) {
    m.seq = seq;
    m.frame = frame;
    std::uint8_t b[kMessageSize];
    encode_message(m, b);
    out.push_back(kMessageSize);
    out.insert(out.end(), b, b + kMessageSize);
}

std::uint32_t h(const char* n) { return nnh::detail::fnv1a32(n); }

} // namespace

NNH_TEST(live_remote_target_survives_mutated_sessions) {
    if constexpr (!nnh::live::kEnabled) return;
    std::vector<std::uint8_t> seed;
    Message hello;
    hello.type = MessageType::Hello;
    hello.version = kVersion;
    add(seed, hello, 1, 0);
    add(seed, live_message(LiveOp::Catalogue, 0, 0, 0), 2, 0);
    add(seed, live_set_message(h("t/f"), nnh::detail::live::float_bits(2.5f)), 3, 3);
    add(seed, live_set_message(h("t/i"), static_cast<std::uint32_t>(-7)), 4, 3);
    add(seed, live_set_message(h("t/c"), 0x11223344u), 5, 4);
    add(seed, live_set_message(h("t/m"), 2), 6, 4);
    add(seed, live_message(LiveOp::Follow, 1, 5, h("w/v")), 7, 5);
    add(seed, live_message(LiveOp::Action, 0, 0, h("t/go")), 8, 5);
    add(seed, pause_message(PauseOp::Pause, 0), 9, 6);
    add(seed, pause_message(PauseOp::Step, 2), 10, 6);
    add(seed, live_message(LiveOp::Reset, 1, 0, h("t")), 11, 8);
    add(seed, pause_message(PauseOp::Resume, 0), 12, 8);
    add(seed, live_message(LiveOp::Dump, 0, 0, 0), 13, 9);
    add(seed, live_message(LiveOp::Reset, 2, 0, 0), 14, 10);
    nnh::fuzz::live_remote_target(seed.data(), seed.size());
    std::uint32_t s = 30030;
    std::vector<std::uint8_t> mutant;
    for (int i = 0; i < 1500; ++i) {
        mutant = seed;
        const int changes = 1 + static_cast<int>(next(s) % 4);
        for (int c = 0; c < changes; ++c) mutant[next(s) % mutant.size()] = static_cast<std::uint8_t>(next(s));
        const std::size_t n = next(s) % 8 == 0 ? next(s) % mutant.size() : mutant.size();
        nnh::fuzz::live_remote_target(mutant.data(), n);
    }
    CHECK(true);
}
