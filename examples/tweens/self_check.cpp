#include "self_check.hpp"

#include <bit>
#include <cstdio>

using nnh::Duration;
using nnh::Ease;
using nnh::Frames;
using nnh::LoopMode;

namespace {

constexpr std::uint32_t kFnvStart = 2166136261u;
constexpr std::uint32_t kFnvPrime = 16777619u;

} // namespace

SelfCheck::SelfCheck() : sum_(kFnvStart) {
    set_.to(&a_, 10.0f, Duration::from_ms(500),
            {.ease = Ease::ElasticOut, .loops = 3, .loop_mode = LoopMode::Yoyo, .on_loop = [this] {
                 record('l');
             }});
    set_.by(&v_, {50, -20}, Frames{37},
            {.ease = Ease::SineInOut, .delay = Frames{7}, .on_finish = [this] { record('v'); }});
    forever_ = set_.from_to(&c_, nnh::Color{0, 0, 0, 255}, nnh::rgb(255, 128, 64), Duration::from_ms(400),
                            {.ease = Ease::BackOut, .loops = nnh::kForever, .loop_mode = LoopMode::Yoyo});
    set_.from_to(nnh::Setter<float>{[this](float v) { setter_value_ = v; }}, 0.0f, 1.0f, Frames{23},
                 {.ease = Ease::BounceOut, .loops = 4});
    set_.sequence({.loops = 2, .loop_mode = LoopMode::Yoyo, .on_finish = [this] { record('S'); }})
        .to(&s1_, 100.0f, Frames{20}, {.ease = Ease::CubicInOut})
        .join()
        .to(&s2_, -5.0f, Frames{13}, {.ease = Ease::QuadIn})
        .wait(Frames{9})
        .call([this] { record('c'); })
        .from_to(&v2_, {1, 2}, {3, 4}, Frames{11}, {.ease = Ease::BackInOut});
    set_.after(Duration::from_seconds(1), [this] { record('a'); });
    set_.every(Duration::from_ms(300), [this] { record('e'); }, 5);
}

void SelfCheck::mix(std::uint32_t word) {
    for (int k = 0; k < 4; ++k) {
        sum_ = (sum_ ^ ((word >> (8 * k)) & 0xFFu)) * kFnvPrime;
    }
}

void SelfCheck::mix(float value) {
    mix(std::bit_cast<std::uint32_t>(value));
}

void SelfCheck::record(char what) {
    mix(static_cast<std::uint32_t>(what) | 0x100u);
}

void SelfCheck::script_frame() {
    switch (frame_) {
    case 150:
        set_.set_speed(0.5f);
        break;
    case 300:
        set_.set_speed(1.5f);
        set_.to(&a_, -3.0f, Frames{40}, {.ease = Ease::ElasticInOut, .on_finish = [this] { record('b'); }});
        break;
    case 420:
        set_.kill(forever_);
        break;
    case 450:
        set_.set_speed(1.0f);
        set_.complete(set_.sequence().by(&s1_, 7.0f, Frames{30}).call([this] { record('x'); }));
        break;
    default:
        break;
    }
}

bool SelfCheck::update() {
    if (done()) return false;
    script_frame();
    set_.update();
    for (int n = shots_.update(); n > 0; --n) {
        record('t');
    }
    if (once_.update() > 0) record('o');
    ++frame_;
    mix(a_);
    mix(v_.x);
    mix(v_.y);
    mix(static_cast<std::uint32_t>(c_.r | c_.g << 8 | c_.b << 16 | c_.a << 24));
    mix(setter_value_);
    mix(s1_);
    mix(s2_);
    mix(v2_.x);
    mix(v2_.y);
    mix(static_cast<std::uint32_t>(set_.active()));
    return frame_ % kCheckpointEvery == 0;
}

bool rounding_probe_ok() {
    volatile float one = 1.0f;
    volatile float tiny = 1.5f * 0x1p-24f; // 0.75 of an ulp of 1: round to nearest goes up, toward zero stays
    const float sum = one + tiny;
    return std::bit_cast<std::uint32_t>(sum) == 0x3F800001u;
}

void format_checkpoint(char* out, std::size_t size, int frame, std::uint32_t sum) {
    std::snprintf(out, size, "tweens: %d sum %08X", frame, static_cast<unsigned>(sum));
}

void format_verdict(char* out, std::size_t size, std::uint32_t sum, bool rounding_ok) {
    if (sum == kSelfCheckGolden && rounding_ok) {
        std::snprintf(out, size, "tweens: PASS %08X", static_cast<unsigned>(sum));
    } else {
        std::snprintf(out, size, "tweens: FAIL sum %08X expected %08X rounding %s",
                      static_cast<unsigned>(sum), static_cast<unsigned>(kSelfCheckGolden),
                      rounding_ok ? "ok" : "WRONG");
    }
}
