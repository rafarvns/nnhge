// AC-2, no wrong frame (spec 023 FR-11): replace, push, pop and cover with every built-in effect, a tall wipe and a
// custom effect; every frame, from the fake platform's submitted items, per screen: the scene on top of the drawing
// is the outgoing one only before the change (with any cover) or under a full cover; the incoming one only under a
// full cover until the in phase starts; the covered phase is fully covered.

#include "nnh/platform/video.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;
using nnh::Screen;
using Item = nnh::platform::DrawItem2D;

namespace {

constexpr nnh::Color kOld = nnh::colors::red;
constexpr nnh::Color kNew = nnh::colors::blue;
constexpr nnh::Color kCover = nnh::rgb(0, 200, 0);

bool same_rgb(nnh::Color a, nnh::Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

struct ScreenView {
    nnh::Color shown{};   ///< the colour of the topmost scene's full-screen rectangle
    bool full = false;    ///< the overlay (opaque, after the scenes) covers every pixel
};

ScreenView view(const Item* items, int count, int width) {
    ScreenView v;
    static unsigned char px[400 * 240];
    for (int i = 0; i < 400 * 240; ++i) px[i] = 0;
    for (int i = 0; i < count; ++i) {
        const Item& it = items[i];
        if (it.kind != Item::Kind::FillRect) continue;
        if (it.layer == 0 && it.w == static_cast<float>(width) && it.h == 240) v.shown = it.color;
        if (it.layer != 15 || !same_rgb(it.color, kCover) || it.color.a != 255) continue;
        for (int y = static_cast<int>(it.y); y < static_cast<int>(it.y + it.h); ++y) {
            for (int x = static_cast<int>(it.x); x < static_cast<int>(it.x + it.w); ++x) px[y * 400 + x] = 1;
        }
    }
    v.full = true;
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < width; ++x) v.full = v.full && px[y * 400 + x] != 0;
    }
    return v;
}

/// The last frame's items split per screen (the top screen's are submitted first, sorted by layer: the bottom
/// screen's start where the layer drops back).
bool frame_ok(tr::Phase phase, bool changed, bool cover_op, int* failures) {
    const fake::State& s = fake::state();
    const int n = s.last_frame_item_count;
    if (n > fake::kMaxRecordedItems) return false;
    int split = 1;
    while (split < n && s.last_frame_items[split].layer >= s.last_frame_items[split - 1].layer) ++split;
    const ScreenView views[2] = {view(s.last_frame_items, split, 400),
                                 view(s.last_frame_items + split, n - split, 320)};
    bool ok = true;
    for (const ScreenView& v : views) {
        const bool old_shown = same_rgb(v.shown, kOld);
        const bool new_shown = same_rgb(v.shown, kNew);
        if (phase == tr::Phase::Covered) ok = ok && v.full;
        if (!cover_op) {
            if (old_shown && changed) ok = ok && v.full;                         // the old scene after the change
            if (new_shown && phase != tr::Phase::In) ok = ok && v.full;          // the new one before the in phase
            if (phase == tr::Phase::Out) ok = ok && old_shown;
        }
    }
    if (!ok) ++*failures;
    return ok;
}

enum class Op { Replace, Push, Pop, Cover };

int run_case(Op op, const tr::Options& options) {
    restart({.gfx = {.max_draws = 2048}});
    Probe old_scene(kOld), new_scene(kNew);
    if (op == Op::Pop) {
        nnh::scenes::push(new_scene); // pop reveals the scene below
        nnh::scenes::push(old_scene);
    } else {
        nnh::scenes::push(old_scene);
    }
    step();
    bool started = false;
    switch (op) {
    case Op::Replace: started = tr::replace(new_scene, options); break;
    case Op::Push: started = tr::push(new_scene, options); break;
    case Op::Pop: started = tr::pop(options); break;
    case Op::Cover: started = tr::cover(options); break;
    }
    CHECK(started);
    int failures = 0;
    bool changed = false;
    bool have_previous = false;
    tr::Phase previous = tr::Phase::None;
    for (int guard = 0; guard < 200; ++guard) {
        CHECK(nnh::frame()); // ends the previous frame: its items are the fake's last frame
        if (have_previous) frame_ok(previous, changed, op == Op::Cover, &failures);
        if (!tr::running()) break;
        nnh::scenes::update_and_draw();
        previous = tr::running() ? tr::phase() : tr::Phase::In; // the last frame is an in frame
        have_previous = true;
        changed = changed || previous == tr::Phase::Covered || previous == tr::Phase::In;
    }
    nnh::shutdown();
    return failures;
}

void custom_cover(const tr::CustomFrame& f) {
    const float w = static_cast<float>(nnh::screen_width(f.screen));
    nnh::gfx::fill_rect(f.screen, {0, 0, f.coverage >= 1.0f ? w : w * f.coverage * 0.5f, 240}, kCover,
                        {.layer = 15, .ignore_camera = true});
}

} // namespace

NNH_TEST(no_frame_shows_the_wrong_scene) {
    const tr::Effect effects[] = {tr::fade(kCover),
                                  tr::wipe(tr::Edge::Left, kCover),
                                  tr::wipe(tr::Edge::Right, kCover),
                                  tr::wipe(tr::Edge::Top, kCover),
                                  tr::wipe(tr::Edge::Bottom, kCover),
                                  tr::split(tr::SplitAxis::LeftRight, kCover),
                                  tr::split(tr::SplitAxis::TopBottom, kCover),
                                  tr::blinds(tr::Edge::Top, 8, kCover),
                                  tr::iris({{120, 60}}, tr::IrisShape::Circle, kCover),
                                  tr::iris({{-30, 260}}, tr::IrisShape::Diamond, kCover, 16)};
    int failures = 0;
    for (Op op : {Op::Replace, Op::Push, Op::Pop, Op::Cover}) {
        for (const tr::Effect& e : effects) {
            for (tr::Reveal style : {tr::Reveal::Reverse, tr::Reveal::Continue}) {
                failures += run_case(op, {.effect = e, .reveal_style = style, .out = Frames{7}, .hold = Frames{2},
                                          .in = Frames{7}, .out_ease = nnh::Ease::BackOut});
            }
        }
        failures += run_case(op, {.effect = tr::wipe(tr::Edge::Top, kCover), .tall = true, .tall_gap = 24,
                                  .out = Frames{9}, .in = Frames{9}});
        failures += run_case(op, {.effect = tr::custom(), .out = Frames{5}, .in = Frames{5}, .custom = custom_cover});
        failures += run_case(op, {.effect = tr::fade(kCover), .out = Frames{0}, .hold = Frames{0}, .in = Frames{0}});
    }
    CHECK(failures == 0);
}
