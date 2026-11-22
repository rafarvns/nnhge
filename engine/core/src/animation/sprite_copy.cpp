// Copying a validated sprite library file into a block with extra capacities (see sprite_copy.hpp).

#include "animation/sprite_copy.hpp"

#include <cstring>

namespace nnh::detail {

using namespace spritefmt;

const char* sprite_loaded_capacities(const Header& file, const int (&extras)[kSectionCount], SpriteCapacities& out,
                                     bool& limit) {
    limit = false;
    for (int s = 0; s < kSectionCount; ++s) {
        if (extras[s] < 0) return "negative extra capacity";
        const long long want = static_cast<long long>(file.count[s]) + extras[s];
        out.cap[s] = want > 0x7FFF ? 0x7FFF : static_cast<int>(want);
    }
    if (out.cap[kFrames] == 0) out.cap[kFrames] = 1;
    if (const char* why = check_sprite_capacities(out)) {
        limit = true;
        return why;
    }
    return nullptr;
}

void copy_sprite_library(const std::uint8_t* file, const SpriteCapacities& caps, std::uint8_t* out) {
    Header h;
    std::memcpy(&h, file, sizeof h);
    const SpriteLayout layout = sprite_layout(caps);
    Header n = h;
    n.serial = 0;
    n.crc = 0;
    n.file_size = layout.total;
    for (int s = 0; s < kSectionCount; ++s) {
        n.cap[s] = static_cast<std::uint16_t>(caps.cap[s]);
        if (s > 0) n.offset[s - 1] = layout.offset[s];
    }
    std::memcpy(out, &n, sizeof n);
    for (int s = 0; s < kSectionCount; ++s) {
        const std::uint32_t from = s == kFrames ? sizeof(Header) : h.offset[s - 1];
        std::memcpy(out + layout.offset[s], file + from,
                    static_cast<std::size_t>(h.count[s]) * static_cast<std::size_t>(kRecordSize[s]));
    }
}

} // namespace nnh::detail
