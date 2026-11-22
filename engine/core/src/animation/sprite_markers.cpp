// Frame markers (spec 016 FR-8; T4.1): kept sorted by clip frame, so the markers of a frame are one run of the
// table; at most 8 per frame, in the order they were added. Names are hashed, not stored.

#include <cstring>

#include "animation/sprite_library_internal.hpp"

namespace nnh {

using namespace detail::spritefmt;

Result<void> SpriteLibrary::add_marker(SpriteClipId clip, int position, const char* name, int value) {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "add_marker: empty library");
    const detail::SpriteView v = detail::sprite_view(block_);
    if (const char* why = detail::sprite_name_problem(name)) {
        return Error::make(ErrorCode::InvalidArgument, "add_marker: %s", why);
    }
    if (clip.value >= v.count(kClips)) return Error::make(ErrorCode::InvalidArgument, "marker \"%s\": unknown clip", name);
    const ClipEntry& c = v.clips()[clip.value];
    if (position < 0 || position >= c.length) {
        return Error::make(ErrorCode::InvalidArgument, "marker \"%s\": clip \"%s\" has no frame %d", name, c.name,
                           position);
    }
    const int at_frame = c.first + position;
    ClipFrameEntry& f = v.clip_frames()[at_frame];
    if (f.markers >= kMaxMarkersPerFrame) {
        return Error::make(ErrorCode::LimitReached, "marker \"%s\": frame %d of clip \"%s\" has 8 markers", name,
                           position, c.name);
    }
    const int count = v.count(kMarkers);
    if (count >= v.cap(kMarkers)) {
        return Error::make(ErrorCode::LimitReached, "marker \"%s\": the library holds %d markers", name,
                           v.cap(kMarkers));
    }
    const int at = f.first_marker + f.markers; // after this frame's markers
    MarkerEntry* m = v.markers();
    std::memmove(m + at + 1, m + at, static_cast<std::size_t>(count - at) * sizeof(MarkerEntry));
    m[at] = {detail::fnv1a32(name), value};
    ++f.markers;
    for (int i = at_frame + 1; i < v.count(kClipFrames); ++i) ++v.clip_frames()[i].first_marker;
    v.header().count[kMarkers] = static_cast<std::uint16_t>(count + 1);
    return {};
}

} // namespace nnh
