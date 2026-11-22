#pragma once
// The sprite library block (spec 016, D31): the same bytes in memory and in a `.nnhsprite` file. A 64-byte header,
// then sections of fixed-size records sized by the header's capacities, each 4-byte aligned (sprite_layout.hpp).
// Little-endian; both the 3DS and the PCs we build on are. Pure (no engine includes): nnh-assetc writes it.

#include <cstdint>

namespace nnh::detail::spritefmt {

inline constexpr char kMagic[4] = {'N', 'N', 'H', 'A'};
inline constexpr std::uint16_t kVersion = 1;

// Limits (the public kSprite* values; animation_limits.cpp checks they agree).
inline constexpr int kMaxFrames = 4096;
inline constexpr int kMaxClips = 256;
inline constexpr int kMaxClipLength = 256;
inline constexpr int kMaxClipFrames = 8192;
inline constexpr int kMaxMarkers = 1024;
inline constexpr int kMaxStates = 32;
inline constexpr int kMaxTransitions = 128;
inline constexpr int kMaxNameLength = 31;
inline constexpr int kMaxMarkersPerFrame = 8;
inline constexpr int kMaxRepeat = 65535;
inline constexpr std::int32_t kTicksPerFrame = 4096;
inline constexpr std::int32_t kMaxTicks = 3600 * 60 * kTicksPerFrame; ///< a frame or a cycle lasts at most 1 hour
inline constexpr int kMaxPivot = 2047; ///< |pivot| in pixels (stored in 1/16 px in an int16)

// Sections, in block order. Frames start right after the header.
enum Section : int { kFrames, kClips, kClipFrames, kMarkers, kStates, kTransitions, kSectionCount };

// Enum values as stored (the public enums' values).
enum Mode : std::uint8_t { kLoop, kOnce, kRepeat, kPingPong };
enum Direction : std::uint8_t { kForward, kReverse };
enum Switch : std::uint8_t { kNow, kAtEnd, kNever };
enum Pivot : std::uint8_t { kTopLeft, kCenter, kBottomCenter };
inline constexpr std::uint8_t kNoState = 0xFF;
inline constexpr std::uint8_t kAnyState = 0xFE;

struct Header {
    char magic[4];
    std::uint16_t version;
    std::uint16_t serial;    ///< memory only: the library's lifetime serial (any value in files; ignored)
    std::uint32_t file_size; ///< the whole block
    std::uint32_t crc;       ///< CRC-32 of bytes [16, file_size); 0 in memory
    std::uint16_t count[kSectionCount];
    std::uint16_t cap[kSectionCount];
    std::uint32_t offset[kSectionCount - 1]; ///< of sections kClips.. (kFrames is at 64)
    std::uint8_t pivot;                      ///< default Pivot
    std::uint8_t reserved[3];
};
static_assert(sizeof(Header) == 64);

struct FrameEntry {
    std::int16_t x, y, w, h;        ///< region in texture pixels
    std::int16_t pivot_x, pivot_y;  ///< 1/16 px, in the untrimmed frame
    std::int16_t offset_x, offset_y; ///< the region's place in the untrimmed frame
};
static_assert(sizeof(FrameEntry) == 16);

struct ClipEntry {
    char name[32];            ///< NUL-terminated, zero-padded
    std::uint32_t hash;       ///< fnv1a32(name)
    std::uint16_t first;      ///< first clip frame (clips' frames follow each other in clip order)
    std::uint16_t length;     ///< 1..256
    std::uint8_t mode;        ///< Mode
    std::uint8_t direction;   ///< Direction
    std::uint16_t count;      ///< Loop 0; Once 1; Repeat plays; PingPong passes (0 = forever)
    std::uint16_t end_step;   ///< finite: the last shown position of the cycle
    std::uint16_t end_cycle;  ///< finite: the cycle it is in
    std::int32_t cycle_ticks; ///< one cycle (ping-pong: a round trip), ≤ kMaxTicks
};
static_assert(sizeof(ClipEntry) == 52);

struct ClipFrameEntry {
    std::int32_t duration;     ///< ticks, 1..kMaxTicks
    std::uint16_t frame;       ///< sheet frame
    std::uint16_t first_marker; ///< markers are sorted by clip frame
    std::uint8_t markers;      ///< 0..8
    std::uint8_t pad[3];
};
static_assert(sizeof(ClipFrameEntry) == 12);

struct MarkerEntry {
    std::uint32_t hash; ///< fnv1a32 of the name (names are not kept)
    std::int32_t value;
};
static_assert(sizeof(MarkerEntry) == 8);

struct StateEntry {
    char name[32];
    std::uint32_t hash;
    std::uint16_t clip;
    std::uint8_t next; ///< next state on finish, kNoState = none
    std::uint8_t pad;
};
static_assert(sizeof(StateEntry) == 40);

struct TransitionEntry {
    std::uint8_t from, to; ///< a state or kAnyState
    std::uint8_t when;     ///< Switch
    std::uint8_t keep_time;
};
static_assert(sizeof(TransitionEntry) == 4);

inline constexpr int kRecordSize[kSectionCount] = {16, 52, 12, 8, 40, 4};

} // namespace nnh::detail::spritefmt
