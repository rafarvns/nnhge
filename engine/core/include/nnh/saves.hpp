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

#include "nnh/data.hpp"
#include "nnh/result.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>

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
        for (std::uint16_t id : ids)
            push(id, false);
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

/// What a successful load did (FR-15). Counts are always kept (saturating at 65535); the first kReportPaths
/// paths of each count only in Debug and Development builds (Release leaves them empty).
struct Report {
    bool used_backup = false;  ///< the file was unusable; its backup (`<path>.bak`) was loaded
    std::uint32_t version = 0; ///< the save version that wrote the file
    int migrated = 0;          ///< migration steps run
    int unknown = 0;           ///< values skipped: ids the struct doesn't declare (retired ids not counted)
    int dropped = 0;           ///< values that didn't fit the current field: it kept its default
    int adjusted = 0;          ///< strings cut at a character boundary, lists and arrays shortened
    FieldPath unknown_at[kReportPaths];
    FieldPath dropped_at[kReportPaths];
    FieldPath adjusted_at[kReportPaths];
};

/// A save file's structured header (FR-16), read without decoding the payload or checking its CRC.
struct FileInfo {
    std::uint32_t key_hash = 0;
    char key[kMaxKeyLength + 1] = {};
    std::uint32_t version = 0; ///< the save version that wrote it
    std::uint32_t size = 0;    ///< payload bytes: what a load's buffer needs
    bool from_backup = false;  ///< the main file is missing or unreadable: this is its backup's header
};

} // namespace nnh::save

// In this order: the records, the kind deduction, the table builders, the macros.
#include "nnh/detail/save_schema.hpp"
// (separate blocks keep the order)
#include "nnh/detail/save_kinds.hpp"
//
#include "nnh/detail/save_tables.hpp"
//
#include "nnh/detail/save_macros.hpp"

namespace nnh::detail {

// The non-template functions the templates below call (engine/core/src/saves/): the codec is compiled once.
Result<std::size_t> save_encode(const SaveTypeInfo& type, const void* value, void* out, std::size_t capacity);
Result<save::Report> save_decode(const SaveTypeInfo& type, const void* data, std::size_t size, void* value);
Result<void> save_store(const SaveTypeInfo& type, const char* path, const void* value, void* buffer,
                        std::size_t capacity);
Result<save::Report> save_load(const SaveTypeInfo& type, const char* path, void* value, void* buffer,
                               std::size_t capacity);
bool old_fields_get(const std::uint8_t* root, std::uint32_t size, const save::FieldPath& path,
                    const SaveFieldInfo& field, void* out);
using SaveLineSink = void (*)(void* context, const char* line);
Result<void> save_dump_value(const SaveTypeInfo& type, const void* value, void* scratch, std::size_t capacity,
                             SaveLineSink sink, void* context);
Result<void> save_dump_bytes(const SaveTypeInfo* type, const void* data, std::size_t size, SaveLineSink sink,
                             void* context);
Result<void> save_dump_file(const SaveTypeInfo* type, const char* path, void* buffer, std::size_t capacity,
                            SaveLineSink sink, void* context);

/// T's record; a build error naming the fix when T has no save declaration.
template <class T> constexpr const SaveTypeInfo& save_info_of() {
    static_assert(save_declared<T>,
                  "this type has no save declaration: add NNH_SAVE_TYPE(Type, \"key\", ...) at "
                  "namespace scope in its namespace");
    return nnh_save_info(SaveTag<T>{});
}

/// The record OldFields::get converts into: a value of type T at offset 0.
template <class T> constexpr SaveFieldInfo save_value_field() {
    return save_field_entry<T>("", 0, 1).field;
}

} // namespace nnh::detail

namespace nnh::save {

/// Read-only view of the file's fields, given to migration steps (FR-13): it reads the original bytes, so
/// retired, unknown and dropped values are all reachable. Valid during the step only. Lookups walk the field
/// headers of the levels they pass (load time, not for every frame); nothing allocates.
/// @code
/// void progress_from_v2(Progress& p, const nnh::save::OldFields& old) {
///     std::uint32_t coins = 0;
///     if (old.get(nnh::save::path(3), coins)) p.gems = coins / 10; // field 3 (retired in v3) held the coins
/// }
/// @endcode
class OldFields {
  public:
    /// Engine use: the root's fields [root, root + size) of a validated file.
    constexpr OldFields(const std::uint8_t* root, std::uint32_t size) : root_(root), size_(size) {}

    /// Whether the file has this field (or item).
    bool has(const FieldPath& path) const;
    /// Its kind in the file; WireKind::None when absent.
    WireKind kind(const FieldPath& path) const;
    /// Its shape in the file (One when absent; an item of a sequence is One).
    WireShape shape(const FieldPath& path) const;
    /// Items of an array or list; -1 for another field or when absent.
    int count(const FieldPath& path) const;
    /// The value converted by the load rules into `out`: a number, bool, enum, Vec2, Color, reference,
    /// data::String<N> (cut at a character boundary when longer), an array or data::List of those, or a
    /// struct with its own save declaration (decoded into T{} by its ids). false, and `out` unchanged, when
    /// absent or when the value doesn't fit the type (out of range, another kind, not finite: FR-11).
    template <class T> bool get(const FieldPath& path, T& out) const {
        static constexpr detail::SaveFieldInfo field = detail::save_value_field<T>();
        T value{}; // a struct, array or list starts from its defaults; `out` changes only on success
        if (!detail::old_fields_get(root_, size_, path, field, &value)) return false;
        if constexpr (std::is_array_v<T>) {
            for (std::size_t i = 0; i < std::extent_v<T>; ++i)
                out[i] = value[i];
        } else {
            out = value;
        }
        return true;
    }

  private:
    const std::uint8_t* root_ = nullptr;
    std::uint32_t size_ = 0;
};

/// The largest encoded size of T (every string, list and array full), structured header included.
/// Compile-time.
template <class T> constexpr std::size_t max_size() {
    constexpr const detail::SaveTypeInfo& type = detail::save_info_of<T>();
    return detail::kSaveHeaderBytes + detail::save_key_length(type.key) + type.max_fields;
}

/// A buffer of exactly max_size<T>() bytes, plus `Headroom` for files written by versions with more data (a
/// load needs room for the whole file). Keep it static or in a scene: the 3DS main thread's stack is small.
template <class T, std::size_t Headroom = 0> struct Buffer {
    alignas(4) std::uint8_t bytes[max_size<T>() + Headroom];
};

/// Encodes `value` into `out` (structured header + fields). Returns the bytes written. Errors, with nothing
/// written past `capacity`: LimitReached (out too small; max_size<T>() always fits), InvalidArgument (a
/// string member isn't UTF-8; an assert in Debug/Development). T needs NNH_SAVE_TYPE (a build error
/// otherwise; summaries can't be encoded). Allocates nothing.
template <class T> Result<std::size_t> encode(const T& value, void* out, std::size_t capacity) {
    static_assert(detail::save_info_of<T>().role == detail::SaveRole::Type,
                  "only an NNH_SAVE_TYPE can be encoded or stored (not a struct or a summary)");
    return detail::save_encode(detail::save_info_of<T>(), &value, out, capacity);
}

/// Decodes an encoded save from memory into `value` (validate, check key and version, T{}, fields by id,
/// steps). `value` is unchanged on any error: Corrupted (damaged structure, with the field's path),
/// InvalidArgument (a save of another type: both keys named), Unsupported (a newer save version or format; a
/// raw nnh::save payload).
template <class T> Result<Report> decode(const void* data, std::size_t size, T& value) {
    static_assert(detail::save_info_of<T>().role != detail::SaveRole::Struct,
                  "decode needs an NNH_SAVE_TYPE or NNH_SAVE_SUMMARY, not a struct");
    return detail::save_decode(detail::save_info_of<T>(), data, size, &value);
}

/// Stores `value` in `path` (spec 002's atomic write: `<path>.tmp`, the previous file kept as `<path>.bak`,
/// CRC-32), encoding it into `buffer` first: one platform write, the same five file operations as
/// nnh::save::write. Errors: InvalidArgument (path; a string member isn't UTF-8), LimitReached (buffer),
/// IoError. In the simulator's Debug and Development builds, NNH_SAVE_DUMP=1 also writes `<path>.txt` (the
/// dump).
template <class T> Result<void> store(const char* path, const T& value, void* buffer, std::size_t capacity) {
    static_assert(detail::save_info_of<T>().role == detail::SaveRole::Type,
                  "only an NNH_SAVE_TYPE can be encoded or stored (not a struct or a summary)");
    return detail::save_store(detail::save_info_of<T>(), path, &value, buffer, capacity);
}
template <class T, std::size_t H> Result<void> store(const char* path, const T& value, Buffer<T, H>& buffer) {
    return store(path, value, buffer.bytes, sizeof buffer.bytes);
}

/// Loads `path` into `value`: the whole file is validated first, then `value` becomes T{}, gets every field
/// the file has by id (converted when it fits), and the migration steps run for an older version. A missing,
/// damaged or unreadable file falls back to `<path>.bak` (the report says so). `value` is unchanged on any
/// error: NotFound (no file and no backup: a new game), Corrupted (both unusable), InvalidArgument (path; a
/// save of another type), Unsupported (a newer version or format; a raw nnh::save file), LimitReached
/// (buffer), IoError. A load that used the backup, dropped or adjusted values logs one warning line.
template <class T> Result<Report> load(const char* path, T& value, void* buffer, std::size_t capacity) {
    static_assert(detail::save_info_of<T>().role != detail::SaveRole::Struct,
                  "load needs an NNH_SAVE_TYPE or NNH_SAVE_SUMMARY, not a struct");
    return detail::save_load(detail::save_info_of<T>(), path, &value, buffer, capacity);
}
template <class T, std::size_t H> Result<Report> load(const char* path, T& value, Buffer<T, H>& buffer) {
    return load(path, value, buffer.bytes, sizeof buffer.bytes);
}

/// The structured header of `path` (else of its backup), without the CRC check: one size and one small read.
/// Errors: NotFound, Corrupted, Unsupported (a raw nnh::save file; a newer format), InvalidArgument (path).
Result<FileInfo> info(const char* path);

/// Whether `path` or its backup exists (false for a bad path).
bool exists(const char* path);

/// Removes `path`, its backup and a temp file left by an interrupted store (each that exists). Succeeds when
/// none existed. Errors: InvalidArgument (path), IoError (one is still there).
Result<void> remove(const char* path);

// ---- Text (FR-17): Debug and Development only; in Release these return Unsupported and hold no dump code
// ----

/// Receives each line of a dump.
using LineSink = void (*)(void* context, const char* line);

/// A sink that logs each line (info level, channel "saves").
void log_lines(void* context, const char* line);

/// Dumps `value` (encoded into `scratch` first; max_size<T>() fits): a first line `save 'progress' v3 (N
/// bytes)`, then one line per field `name (id) kind = value`, nested structs indented, lists with their
/// count.
template <class T>
Result<void> dump(const T& value, void* scratch, std::size_t capacity, LineSink sink,
                  void* context = nullptr) {
    return detail::save_dump_value(detail::save_info_of<T>(), &value, scratch, capacity, sink, context);
}

/// Dumps an encoded save in memory with T's names; ids it doesn't know as `#id kind`, retired ids marked.
template <class T>
Result<void> dump_bytes(const void* data, std::size_t size, LineSink sink, void* context = nullptr) {
    return detail::save_dump_bytes(&detail::save_info_of<T>(), data, size, sink, context);
}

/// Dumps an encoded save knowing only its ids (no schema).
inline Result<void> dump_bytes(const void* data, std::size_t size, LineSink sink, void* context = nullptr) {
    return detail::save_dump_bytes(nullptr, data, size, sink, context);
}

/// Dumps a save file (its backup when it is unusable) with T's names, reading it into `buffer`.
template <class T>
Result<void> dump_file(const char* path, void* buffer, std::size_t capacity, LineSink sink,
                       void* context = nullptr) {
    return detail::save_dump_file(&detail::save_info_of<T>(), path, buffer, capacity, sink, context);
}

} // namespace nnh::save
