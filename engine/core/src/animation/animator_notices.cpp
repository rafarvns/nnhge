// Notices (spec 016 FR-8, FR-9; T4.2, T4.3): the module's scratch buffer, emitting with the callback, the markers of
// a frame, and the bounds.

#include "animation/animator_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh {

using namespace detail;
using namespace detail::spritefmt;
using Access = SpriteAnimatorAccess;

namespace detail {

namespace {
NoticeScratch g_scratch; // 512 B + 8, .bss: linked only with the module
} // namespace

NoticeScratch& notice_scratch() {
    return g_scratch;
}

void animator_recover_after_crash() {
    notice_scratch().callbacks_running = 0;
}

} // namespace detail

void Access::report_bound(Pass& p) {
    p.silent = true;
    NNH_LOG_ERROR(animation, "SpriteAnimator: one update crossed more than %d frames or made more than %d notices "
                             "(the rest of its notices are skipped)",
                  kSpriteMaxCrossings, kSpriteMaxNotices);
    NNH_ASSERT(false, "nnh::SpriteAnimator: more than %d frames or notices in one update", kSpriteMaxCrossings);
}

void Access::emit(SpriteAnimator& a, Pass& p, const SpriteNotice& n) {
    if (p.silent) return;
    NoticeScratch& s = notice_scratch();
    if (s.count >= kSpriteMaxNotices) {
        report_bound(p);
        return;
    }
    s.items[s.count++] = n;
    if (!a.callback_) return;
    ++s.callbacks_running;
    a.callback_(a, n);
    --s.callbacks_running;
}

void Access::emit_simple(SpriteAnimator& a, Pass& p, SpriteNoticeKind kind) {
    SpriteNotice n;
    n.kind = kind;
    n.clip = SpriteClipId{a.clip_};
    n.state = SpriteStateId{a.state_};
    n.previous = SpriteStateId{a.previous_};
    emit(a, p, n);
}

void Access::emit_markers(SpriteAnimator& a, Pass& p) {
    const ClipFrameEntry& f = clip_frame(a);
    if (f.markers == 0 || p.silent) return;
    const SpriteView v = sprite_view(a.block_);
    const ClipEntry& c = clip(a);
    const int first = f.first_marker;
    const int count = f.markers;
    const auto position = static_cast<std::uint16_t>(&f - (v.clip_frames() + c.first));
    const std::uint16_t clip_id = a.clip_;
    for (int i = 0; i < count; ++i) {
        const MarkerEntry& m = v.markers()[first + i];
        SpriteNotice n;
        n.kind = SpriteNoticeKind::Marker;
        n.clip = SpriteClipId{clip_id};
        n.state = SpriteStateId{a.state_};
        n.previous = SpriteStateId{a.previous_};
        n.position = position;
        n.marker = m.hash;
        n.value = m.value;
        emit(a, p, n);
        if (changed(a, p)) return; // the callback changed the clip: the rest belong to the old one
    }
}

} // namespace nnh
