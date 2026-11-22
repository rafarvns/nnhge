// Sprite library storage (spec 016 FR-5, T1.2): one block per library (tag `animation`), lifetime serials and the
// Debug/Development live table, move, counts, names.

#include <cstring>
#include <utility>

#include "animation/sprite_layout.hpp"
#include "animation/sprite_library_internal.hpp"
#include "nnh/assert.hpp"
#include "nnh/build.hpp"
#include "text/utf8.hpp"

NNH_MEM_TAG(animation);
NNH_LOG_CHANNEL(animation);

namespace nnh {

// The format's limits are the public ones.
static_assert(kSpriteMaxFrames == detail::spritefmt::kMaxFrames && kSpriteMaxClips == detail::spritefmt::kMaxClips &&
              kSpriteMaxClipLength == detail::spritefmt::kMaxClipLength &&
              kSpriteMaxClipFrames == detail::spritefmt::kMaxClipFrames &&
              kSpriteMaxMarkers == detail::spritefmt::kMaxMarkers && kSpriteMaxStates == detail::spritefmt::kMaxStates &&
              kSpriteMaxTransitions == detail::spritefmt::kMaxTransitions &&
              kSpriteMaxNameLength == detail::spritefmt::kMaxNameLength &&
              kSpriteMaxMarkersPerFrame == detail::spritefmt::kMaxMarkersPerFrame &&
              kSpriteMaxRepeat == detail::spritefmt::kMaxRepeat);
static_assert(sizeof(SpriteNotice) == 16);

namespace detail {
namespace {

std::uint16_t g_last_serial = 0;

#if !defined(NNH_FLAVOR_RELEASE)
constexpr int kLiveSlots = 1024;
std::uint16_t g_live[kLiveSlots] = {}; // serial of the live library in slot serial & 1023, 0 = free
#endif

std::uint16_t next_serial() {
    if (++g_last_serial == 0) g_last_serial = 1;
    return g_last_serial;
}

} // namespace

MemTag& animation_memory_tag() {
    return NNH_MEM_TAG_REF(animation);
}

std::uint16_t sprite_take_serial() {
#if !defined(NNH_FLAVOR_RELEASE)
    for (int tries = 0; tries < 0x10000; ++tries) {
        const std::uint16_t s = next_serial();
        if (g_live[s & (kLiveSlots - 1)] == 0) {
            g_live[s & (kLiveSlots - 1)] = s;
            return s;
        }
    }
    NNH_PANIC("nnh::SpriteLibrary: more than 1023 libraries alive at once");
#else
    return next_serial();
#endif
}

void sprite_release_serial(std::uint16_t serial) {
#if !defined(NNH_FLAVOR_RELEASE)
    if (g_live[serial & (kLiveSlots - 1)] == serial) g_live[serial & (kLiveSlots - 1)] = 0;
#else
    (void)serial;
#endif
}

bool sprite_serial_live(std::uint16_t serial) {
#if !defined(NNH_FLAVOR_RELEASE)
    return serial != 0 && g_live[serial & (kLiveSlots - 1)] == serial;
#else
    (void)serial;
    return true;
#endif
}

const char* sprite_name_problem(const char* name) {
    if (name == nullptr || name[0] == '\0') return "the name is empty";
    const std::size_t length = std::strlen(name);
    if (length > static_cast<std::size_t>(spritefmt::kMaxNameLength)) return "the name is over 31 bytes";
    if (!utf8_valid(name, length)) return "the name is not valid UTF-8";
    return nullptr;
}

void sprite_store_name(char out[32], const char* name) {
    std::memset(out, 0, 32);
    std::memcpy(out, name, std::strlen(name));
}

} // namespace detail

SpriteLibrary SpriteLibraryAccess::adopt(detail::SpriteBlock* block) {
    block->header.serial = detail::sprite_take_serial();
    SpriteLibrary library;
    library.block_ = block;
    return library;
}

Result<SpriteLibrary> SpriteLibrary::create(const SpriteLibraryDesc& d) {
    using namespace detail::spritefmt;
    detail::SpriteCapacities caps;
    const int values[kSectionCount] = {d.frames, d.clips, d.clip_frames, d.markers, d.states, d.transitions};
    for (int s = 0; s < kSectionCount; ++s) caps.cap[s] = values[s];
    if (const char* why = detail::check_sprite_capacities(caps)) {
        return Error::make(ErrorCode::InvalidArgument, "SpriteLibrary: %s", why);
    }
    if (static_cast<int>(d.pivot) > kBottomCenter) {
        return Error::make(ErrorCode::InvalidArgument, "SpriteLibrary: unknown default pivot");
    }
    const detail::SpriteLayout layout = detail::sprite_layout(caps);
    void* memory = mem::alloc(MemRegion::Heap, detail::animation_memory_tag(), layout.total, 8);
    if (memory == nullptr) return Error::make(ErrorCode::OutOfMemory, "SpriteLibrary: no memory for %u bytes",
                                              static_cast<unsigned>(layout.total));
    std::memset(memory, 0, layout.total);
    auto* block = static_cast<detail::SpriteBlock*>(memory);
    Header& h = block->header;
    std::memcpy(h.magic, kMagic, sizeof h.magic);
    h.version = kVersion;
    h.file_size = layout.total;
    for (int s = 0; s < kSectionCount; ++s) {
        h.cap[s] = static_cast<std::uint16_t>(caps.cap[s]);
        if (s > 0) h.offset[s - 1] = layout.offset[s];
    }
    h.pivot = static_cast<std::uint8_t>(d.pivot);
    return SpriteLibraryAccess::adopt(block);
}

SpriteLibrary::~SpriteLibrary() {
    if (block_ == nullptr) return;
    detail::sprite_release_serial(block_->header.serial);
    mem::free(block_);
}

SpriteLibrary::SpriteLibrary(SpriteLibrary&& other) noexcept : block_(std::exchange(other.block_, nullptr)) {}

SpriteLibrary& SpriteLibrary::operator=(SpriteLibrary&& other) noexcept {
    if (this != &other) {
        SpriteLibrary old;
        old.block_ = std::exchange(block_, std::exchange(other.block_, nullptr));
    }
    return *this;
}

int SpriteLibrary::frame_count() const {
    return block_ != nullptr ? block_->header.count[detail::spritefmt::kFrames] : 0;
}
int SpriteLibrary::clip_count() const {
    return block_ != nullptr ? block_->header.count[detail::spritefmt::kClips] : 0;
}
int SpriteLibrary::state_count() const {
    return block_ != nullptr ? block_->header.count[detail::spritefmt::kStates] : 0;
}
std::size_t SpriteLibrary::memory_used() const {
    return block_ != nullptr ? block_->header.file_size : 0;
}

} // namespace nnh
