#pragma once
/// @file nnh/saves.hpp
/// Structured saves with versions (spec 027, decisions D45 and D46): a game describes its save struct once,
/// giving each saved member a stable numeric id, and stores and loads it with one call. Fields can be added,
/// removed, renamed, reordered and widened between releases without code; a save version and ordered
/// migration steps cover the rest. The file is the engine's own layout (little-endian, no padding), so the
/// same save loads on PC and 3DS, inside spec 002's `nnh::save` container (CRC-32, atomic write, `.bak`).
/// Optional (constitution XI): a game that doesn't include this header pays nothing, and raw
/// `nnh::save::read`/`write` keep working unchanged. Reference: docs/api/saves.md; example: examples/saves.
///
/// @code
/// struct Progress {
///     nnh::data::String<24> name;
///     std::uint16_t level = 1;
///     std::uint64_t play_frames = 0;
/// };
/// // At namespace scope, in Progress's own namespace (a header, or one source file):
/// NNH_SAVE_TYPE(Progress, "progress", NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2),
///               NNH_SAVE_FIELD(play_frames, 5));
///
/// nnh::save::Buffer<Progress> buffer;   // max_size<Progress>() bytes: keep it static or in a scene
/// nnh::save::store("slot1.sav", progress, buffer);
/// if (auto r = nnh::save::load("slot1.sav", progress, buffer); !r) start_new_game();
/// @endcode
///
/// Nothing here allocates or calls a virtual function. Stores and loads touch the SD card: call them at a
/// save point or in a menu, not every frame (an Old 3DS store takes tens of milliseconds).

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>

#include "nnh/data.hpp"
#include "nnh/result.hpp"

namespace nnh::save {

inline constexpr std::size_t kMaxEncodedSize = 256 * 1024; ///< an encoded save, structured header included
inline constexpr int kMaxFields = 128;                     ///< saved fields per struct
inline constexpr int kMaxDepth = 8;                        ///< struct levels, the root included
inline constexpr std::size_t kMaxRootBytes = 64 * 1024;    ///< sizeof a save type
inline constexpr int kMaxArrayItems = 65535;               ///< T[N]; data::List keeps its 256
inline constexpr int kMaxId = 65535;                       ///< field ids are 1..65535, unique per struct
inline constexpr int kMaxRetired = 64;                     ///< retired ids per struct
inline constexpr int kMaxKeyLength = 16;                   ///< a save type's key: 1-16 of a-z 0-9 _
inline constexpr int kReportPaths = 3;                     ///< paths kept per count (Debug/Development)
inline constexpr int kMaxPathSteps = 16;                   ///< ids and item indexes in a FieldPath
inline constexpr std::uint16_t kFormatVersion = 1;         ///< the structured format this engine writes

/// What one value of a field is in a file (the low 5 bits of its wire byte). Numbers are part of the file
/// format: never renumbered. An enum is stored as its underlying integer kind.
enum class WireKind : std::uint8_t {
    None, ///< not a field (absent)
    Bool,
    U8,
    I8,
    U16,
    I16,
    U32,
    I32,
    U64,
    I64,
    F32,
    F64,
    Vec2,
    Color,
    DataRef,
    TextureRef,
    SoundRef,
    MusicRef,
    String,
    Struct,
};

/// How many values a field holds in a file: one, a fixed array (T[N]) or a list (data::List<T, N>).
enum class WireShape : std::uint8_t { One, Array, List };

/// A field inside a save, for migration steps and reports: ids from the root, and item indexes of arrays and
/// lists, at most kMaxPathSteps steps. `save::path(9).item(3).field(2)` is item 3 of field 9, then its field
/// 2, written "9[3].2". A path that grew past kMaxPathSteps is invalid and names nothing.
class FieldPath {
public:
    constexpr FieldPath() = default;
    constexpr FieldPath(std::initializer_list<std::uint16_t> ids) {
        for (std::uint16_t id : ids) push(id, false);
    }
    /// This path followed by item `index` (0-based) of the array or list it names.
    constexpr FieldPath item(int index) const {
        FieldPath p = *this;
        p.push(index < 0 || index > 0xFFFF ? 0xFFFF : static_cast<std::uint16_t>(index), true);
        if (index < 0 || index > 0xFFFF) p.invalid_ = true;
        return p;
    }
    /// This path followed by field `id` of the struct it names.
    constexpr FieldPath field(std::uint16_t id) const {
        FieldPath p = *this;
        p.push(id, false);
        return p;
    }
    /// Steps in the path (0 for an empty one).
    constexpr int size() const { return count_; }
    /// False once a step didn't fit (more than kMaxPathSteps, or a negative item index).
    constexpr bool valid() const { return !invalid_; }
    /// Whether step `step` is an item index (else a field id).
    constexpr bool is_item(int step) const { return ((item_mask_ >> step) & 1u) != 0; }
    /// Step `step`: a field id or an item index.
    constexpr std::uint32_t at(int step) const { return steps_[step]; }
    /// Writes "9[3].2" into `out` (always NUL-terminated when capacity > 0). Returns the length written.
    int format(char* out, std::size_t capacity) const;

    friend constexpr bool operator==(const FieldPath& a, const FieldPath& b) {
        if (a.count_ != b.count_ || a.item_mask_ != b.item_mask_ || a.invalid_ != b.invalid_) return false;
        for (int i = 0; i < a.count_; ++i) {
            if (a.steps_[i] != b.steps_[i]) return false;
        }
        return true;
    }

    /// Engine use: drops the last step (the decoder's path stack).
    constexpr void pop() {
        if (count_ > 0) {
            --count_;
            item_mask_ = static_cast<std::uint16_t>(item_mask_ & ~(1u << count_));
            steps_[count_] = 0;
        }
    }
    /// Engine use: appends a step in place.
    constexpr void push(std::uint16_t value, bool is_item_step) {
        if (count_ >= kMaxPathSteps) {
            invalid_ = true;
            return;
        }
        steps_[count_] = value;
        if (is_item_step) item_mask_ = static_cast<std::uint16_t>(item_mask_ | (1u << count_));
        ++count_;
    }

private:
    std::uint16_t steps_[kMaxPathSteps] = {}; ///< an id, or an item index when its bit in item_mask_ is set
    std::uint16_t item_mask_ = 0;
    std::uint8_t count_ = 0;
    bool invalid_ = false;
};

/// A path of field ids: `save::path(4)`, `save::path(4, 2)` (field 2 inside struct field 4).
template <class... Ids> constexpr FieldPath path(Ids... ids) {
    static_assert(sizeof...(Ids) >= 1 && sizeof...(Ids) <= kMaxPathSteps, "a path has 1 to 16 ids");
    return FieldPath{static_cast<std::uint16_t>(ids)...};
}

} // namespace nnh::save
