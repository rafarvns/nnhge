// Frame sequences of clips (see sprite_sequence.hpp).

#include "animation/sprite_sequence.hpp"

#include "animation/sprite_format.hpp"

namespace nnh::detail {

using namespace spritefmt;

SpriteSequenceEnd sprite_sequence_end(int frames, std::uint8_t mode, int count) {
    SpriteSequenceEnd end;
    if (mode == kLoop || count <= 0) return end;
    std::int64_t shown = 0;
    if (mode == kPingPong) {
        shown = frames >= 2 ? 1 + static_cast<std::int64_t>(count) * (frames - 1) : count;
    } else {
        shown = static_cast<std::int64_t>(frames) * count;
    }
    const std::int64_t length = sprite_cycle_length(frames, mode);
    end.finite = true;
    end.end_cycle = static_cast<std::uint16_t>((shown - 1) / length);
    end.end_step = static_cast<std::uint16_t>((shown - 1) % length);
    return end;
}

} // namespace nnh::detail
