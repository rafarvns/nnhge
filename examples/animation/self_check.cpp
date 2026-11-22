#include "self_check.hpp"

#include <cstdio>
#include <nnh/time.hpp>

using nnh::SpriteAnimator;
using nnh::SpriteClipId;
using nnh::SpriteNotice;

namespace {

constexpr std::uint32_t kFnvStart = 2166136261u;
constexpr std::uint32_t kFnvPrime = 16777619u;
constexpr int kGalleryAnimators = 32;
const float kSpeeds[4] = {1.0f, 0.5f, 1.5f, 0.0f};

SpriteClipId gallery_clip(int k) {
    return SpriteClipId{static_cast<std::uint16_t>(k % kGalleryClips)};
}

} // namespace

AnimationSelfCheck::AnimationSelfCheck() : sum_(kFnvStart) {
    for (int i = 0; i < kAnimators; ++i) start(i);
}

void AnimationSelfCheck::start(int i) {
    SpriteAnimator& a = animators_[i];
    if (i < kGalleryAnimators) {
        a = SpriteAnimator{gallery_};
        a.play(gallery_clip(i));
        a.set_speed(kSpeeds[i % 4]);
        a.set_flip(i % 3 == 0);
        if (i % 6 == 1) {
            // Re-entrancy: on frame 4 of its clip, a third of these jump to the next clip at once.
            a.on_notice([](SpriteAnimator& self, const SpriteNotice& n) {
                if (n.kind == nnh::SpriteNoticeKind::Marker && n.value == 4) {
                    self.play(gallery_clip(n.clip.value + 1));
                }
            });
        }
    } else {
        a = SpriteAnimator{knight_, i % 2 == 0 ? states_.idle : states_.run};
        a.set_speed(i % 5 == 0 ? 1.5f : 1.0f);
    }
}

void AnimationSelfCheck::request(int i) {
    // The game's own conditions: a pattern that walks, runs, jumps, falls and attacks per animator.
    const int t = (frame_ + i * 7) % 120;
    SpriteAnimator& a = animators_[i];
    if (t == 50 || t == 51) {
        a.request(states_.attack);
    } else if (t == 90 && i % 3 == 0) {
        a.request(states_.hurt);
    } else if (t >= 70 && t < 80) {
        a.request(t < 74 ? states_.jump : states_.fall);
    } else if (t >= 20 && t < 60) {
        a.request(t % 16 < 8 || i % 4 != 0 ? states_.run : states_.run_shoot);
    } else {
        a.request(states_.idle);
    }
}

void AnimationSelfCheck::script(int i) {
    SpriteAnimator& a = animators_[i];
    switch (frame_) {
    case 100:
        if (i < kGalleryAnimators && i % 3 == 0) (void)a.queue(gallery_clip(i + 2));
        break;
    case 200:
        if (i % 4 == 3) a.set_speed(1.0f); // the frozen ones start
        break;
    case 250:
        if (i < kGalleryAnimators && i % 5 == 0) a.set_frame(3);
        break;
    case 300:
        if (i >= kGalleryAnimators && i % 7 == 0) a.force(states_.hurt);
        break;
    case 400:
        if (i < kGalleryAnimators && i % 2 == 0) a.play(kGalleryNames[(i / 2) % kGalleryClips], true);
        break;
    case 480:
        if (i % 9 == 0) a.pause();
        break;
    case 520:
        if (i % 9 == 0) a.resume();
        break;
    default:
        break;
    }
}

void AnimationSelfCheck::mix(std::uint32_t word) {
    for (int k = 0; k < 4; ++k) sum_ = (sum_ ^ ((word >> (8 * k)) & 0xFFu)) * kFnvPrime;
}

bool AnimationSelfCheck::update() {
    if (done()) return false;
    const nnh::FrameTime step = nnh::Duration{16'666'667}; // time::FixedStep(60).step()
    for (int i = 0; i < kAnimators; ++i) {
        script(i);
        if (i >= kGalleryAnimators) request(i);
        SpriteAnimator& a = animators_[i];
        const nnh::SpriteNotices notices = i % 8 == 7 ? a.advance(step) : a.update();
        for (const SpriteNotice& n : notices) {
            ++notices_;
            mix(static_cast<std::uint32_t>(n.kind) | static_cast<std::uint32_t>(n.state.value) << 8 |
                static_cast<std::uint32_t>(n.clip.value) << 16);
            mix(n.marker ^ static_cast<std::uint32_t>(n.value) ^ static_cast<std::uint32_t>(n.position) << 24);
        }
        mix(static_cast<std::uint32_t>(a.frame() + 1) | static_cast<std::uint32_t>(a.loops()) << 16);
        mix(static_cast<std::uint32_t>(a.time_in_frame().ticks));
    }
    ++frame_;
    return frame_ % kCheckpointEvery == 0;
}

void format_animation_checkpoint(char* out, std::size_t size, int frame, std::uint32_t sum) {
    std::snprintf(out, size, "animation: %d sum %08X", frame, static_cast<unsigned>(sum));
}

void format_animation_verdict(char* out, std::size_t size, int frame, std::uint32_t sum) {
    if (sum == kAnimationSelfCheckGolden) {
        std::snprintf(out, size, "animation: PASS %08X", static_cast<unsigned>(sum));
    } else {
        std::snprintf(out, size, "animation: FAIL %d %08X %08X", frame, static_cast<unsigned>(sum),
                      static_cast<unsigned>(kAnimationSelfCheckGolden));
    }
}
