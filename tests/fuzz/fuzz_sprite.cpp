// Fuzz target body for `.nnhsprite` sprite libraries (spec 016 FR-16, T6.3; see fuzz_targets.hpp): validate, copy
// into a library with extra room, then play every clip and every state of what was accepted.

#include "fuzz_targets.hpp"

#include <cstdlib>
#include <cstring>
#include <utility>

#include "animation/sprite_format.hpp"
#include "animation/sprite_library_internal.hpp"
#include "animation/sprite_load.hpp"
#include "animation/sprite_validate.hpp"
#include "nnh/animation.hpp"
#include "nnh/gfx.hpp"

namespace nnh::fuzz {

namespace {

using namespace detail::spritefmt;

/// The shortest clip frame: advancing by it crosses at most one frame per call, so the bounds are never reached.
std::int32_t shortest_frame(const SpriteLibrary& lib) {
    const detail::SpriteView v = detail::sprite_view(SpriteLibraryAccess::block(lib));
    std::int32_t shortest = FrameTime::kTicksPerFrame;
    for (int i = 0; i < v.count(kClipFrames); ++i) {
        if (v.clip_frames()[i].duration < shortest) shortest = v.clip_frames()[i].duration;
    }
    return shortest;
}

void check_frame(const SpriteLibrary& lib, const SpriteAnimator& a) {
    if (a.frame() < -1 || a.frame() >= lib.frame_count()) std::abort();
    if (a.clip().valid() && (a.position() < 0 || a.position() >= lib.clip_length(a.clip()))) std::abort();
    if (a.progress() < 0.0f || a.progress() > 1.0f) std::abort();
    (void)a.draw_options();
    (void)a.region();
    (void)a.pivot();
}

/// About 30 frames of play in steps of the shortest frame (at most 600 calls).
void play_for_a_while(const SpriteLibrary& lib, SpriteAnimator& a, std::int32_t step, int request_states) {
    const int calls = 30 * FrameTime::kTicksPerFrame / step < 600 ? 30 * FrameTime::kTicksPerFrame / step : 600;
    for (int i = 0; i < calls; ++i) {
        if (request_states > 0) a.request(SpriteStateId{static_cast<std::uint8_t>((i / 7) % request_states)});
        const SpriteNotices n = a.advance(FrameTime::from_ticks(step));
        if (n.count < 0 || n.count > kSpriteMaxNotices) std::abort();
        for (const SpriteNotice& x : n) {
            if (x.clip.value >= lib.clip_count()) std::abort();
        }
        check_frame(lib, a);
    }
}

void play_everything(SpriteLibrary& lib) {
    const std::int32_t step = shortest_frame(lib);
    for (int f = 0; f < lib.frame_count(); ++f) {
        if (lib.frame(f).region.empty()) std::abort();
        (void)lib.draw_options(f, true, f % 2 == 0);
    }
    for (int c = 0; c < lib.clip_count(); ++c) {
        const SpriteClipId id{static_cast<std::uint16_t>(c)};
        if (lib.find_clip(lib.clip_name(id)).value > c) std::abort(); // a name it lists is found (first hash)
        SpriteAnimator a{lib};
        a.play(id);
        (void)a.queue(SpriteClipId{static_cast<std::uint16_t>((c + 1) % lib.clip_count())});
        play_for_a_while(lib, a, step, 0);
    }
    for (int s = 0; s < lib.state_count(); ++s) {
        SpriteAnimator a{lib, SpriteStateId{static_cast<std::uint8_t>(s)}};
        play_for_a_while(lib, a, step, lib.state_count());
    }
    if (lib.clip_count() > 0) { // the extra room: a marker, a state and a rule added in code
        (void)lib.add_marker(SpriteClipId{0}, 0, "fuzz", 1);
        auto state = lib.add_state("fuzz-state", SpriteClipId{0});
        if (state) (void)lib.add_transition({.from = state.value(), .when = SpriteSwitch::AtEnd});
    }
}

} // namespace

int sprite_library_target(const std::uint8_t* data, std::size_t size) {
    // As is (the header and CRC paths), then with the size and CRC fixed so the content checks run.
    (void)detail::validate_sprite_library(data, size);
    static std::uint8_t file[256 * 1024];
    if (size < sizeof(Header) || size > sizeof file) return 0;
    std::memcpy(file, data, size);
    Header h;
    std::memcpy(&h, file, sizeof h);
    h.file_size = static_cast<std::uint32_t>(size);
    std::memcpy(file, &h, sizeof h);
    h.crc = detail::sprite_file_crc(file, size);
    std::memcpy(file, &h, sizeof h);
    if (!detail::validate_sprite_library(file, size).ok()) return 0;
    auto made = detail::sprite_library_from_file(file, size, {.extra_markers = 1, .extra_states = 1,
                                                             .extra_transitions = 1},
                                                 "fuzz");
    if (!made) return 0; // the extras can push a library over a ceiling
    SpriteLibrary lib = std::move(made.value());
    play_everything(lib);
    return 0;
}

} // namespace nnh::fuzz
