// Section layout of a sprite library block (see sprite_layout.hpp).

#include "animation/sprite_layout.hpp"

namespace nnh::detail {

using namespace spritefmt;

const char* check_sprite_capacities(const SpriteCapacities& c) {
    if (c.cap[kFrames] < 1 || c.cap[kFrames] > kMaxFrames) return "frames must be 1..4096";
    if (c.cap[kClips] < 0 || c.cap[kClips] > kMaxClips) return "clips must be 0..256";
    if (c.cap[kClipFrames] < 0 || c.cap[kClipFrames] > kMaxClipFrames) return "clip_frames must be 0..8192";
    if (c.cap[kMarkers] < 0 || c.cap[kMarkers] > kMaxMarkers) return "markers must be 0..1024";
    if (c.cap[kStates] < 0 || c.cap[kStates] > kMaxStates) return "states must be 0..32";
    if (c.cap[kTransitions] < 0 || c.cap[kTransitions] > kMaxTransitions) return "transitions must be 0..128";
    return nullptr;
}

SpriteLayout sprite_layout(const SpriteCapacities& c) {
    SpriteLayout l;
    std::uint32_t at = sizeof(Header);
    for (int s = 0; s < kSectionCount; ++s) {
        l.offset[s] = at; // every record size is a multiple of 4: offsets stay 4-byte aligned
        at += static_cast<std::uint32_t>(kRecordSize[s]) * static_cast<std::uint32_t>(c.cap[s]);
    }
    l.total = at;
    return l;
}

} // namespace nnh::detail
