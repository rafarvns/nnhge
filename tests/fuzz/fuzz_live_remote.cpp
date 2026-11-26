// The live values' remote fuzz target (spec 030 T5.3, FR-18, X, AC-5): bytes from the PC into a Debug/Development
// game whose registry holds a tunable of every kind (with guard bytes around each variable) and watches, frame after
// frame, pauses included. Invariants: every tunable stays in its range, the guards stay intact, nothing crashes.
// Shared by the mini-fuzz in nnh_tests (test_fuzz_live_remote.cpp) and libfuzzer_live_remote.cpp.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "nnh/app.hpp"
#include "nnh/gfx.hpp"
#include "nnh/live.hpp"
#include "nnh/platform_fake.hpp"

namespace nnh::fuzz {
namespace {

namespace fake = platform_fake;
constexpr std::uint32_t kGuard = 0x5AA5C33Cu;

enum class Mode : std::uint8_t { A, B, C };
constexpr const char* kModes[] = {"a", "b", "c"};

struct Vars {
    std::uint32_t g0 = kGuard;
    float f = 1;
    std::uint32_t g1 = kGuard;
    int i = 3;
    std::uint32_t g2 = kGuard;
    bool b = false;
    std::uint8_t pad[3] = {};
    std::uint32_t g3 = kGuard;
    Color c{1, 2, 3, 4};
    std::uint32_t g4 = kGuard;
    Mode m = Mode::B;
    std::uint8_t pad2[3] = {};
    std::uint32_t g5 = kGuard;
    Vec2 watched{1, 2};
};

void check(const Vars& v) {
    if (v.g0 != kGuard || v.g1 != kGuard || v.g2 != kGuard || v.g3 != kGuard || v.g4 != kGuard || v.g5 != kGuard) {
        std::abort();
    }
    if (!(v.f >= -4 && v.f <= 4) || v.i < -10 || v.i > 10 || static_cast<int>(v.m) > 2) std::abort();
}

} // namespace

int live_remote_target(const std::uint8_t* data, std::size_t size) {
    if constexpr (!live::kEnabled) return 0;
    nnh::shutdown();
    fake::reset();
    if (!nnh::init({})) return 0;
    static Vars v;
    v = Vars{};
    {
        live::Handle hf = live::tune("t/f", &v.f, {.min = -4, .max = 4, .step = 0.5f});
        live::Handle hi = live::tune("t/i", &v.i, {.min = -10, .max = 10, .step = 1});
        live::Handle hb = live::tune("t/b", &v.b);
        live::Handle hc = live::tune("t/c", &v.c);
        live::Handle hm = live::tune("t/m", &v.m, kModes);
        live::Handle ha = live::action("t/go", [](void*) {});
        live::Handle hw = live::watch("w/v", &v.watched);
        fake::remote_connect();
        // The input: chunks of `n` bytes (first byte: n & 63), then a frame. Every message is pushed as it comes; a
        // pause that nothing resumes ends after 64 paused ticks (the fake then says quit) and the run stops there.
        std::size_t at = 0;
        bool running = true;
        while (at < size && running) {
            const std::size_t n = data[at++] & 63u;
            const std::size_t take = n < size - at ? n : size - at;
            fake::remote_push(data + at, take);
            at += take;
            fake::quit_after_polls(64);
            running = nnh::frame();
            live::show("w/n", static_cast<int>(at));
            check(v);
        }
        fake::set_quit_requested(false);
    }
    check(v);
    nnh::shutdown();
    fake::reset();
    return 0;
}

} // namespace nnh::fuzz
