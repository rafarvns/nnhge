#pragma once
/// @file nnh/data.hpp
/// Data assets (spec 014): typed game data — enemies, items, a level's settings — kept in hand-written text
/// files in the assets folder, checked by the build against a plain C++ struct and loaded into it by path.
/// Optional (constitution XI): a game that doesn't use it links no data code.
///
/// This header holds the value types data structs are made of (`String`, `List`, references), the per-field
/// options and, through <nnh/detail/data_schema.hpp>, the macros that declare a struct's schema:
/// @code
/// struct Item {
///     nnh::data::String<24> name;
///     int price = 10;
///     nnh::data::TextureRef icon;
/// };
/// // In a schema source (listed in nnh_add_game(... DATA_SCHEMA ...)), at namespace scope:
/// NNH_DATA_TYPE(Item, "item", NNH_DATA_FIELD(name), NNH_DATA_FIELD(price, .min = 0), NNH_DATA_FIELD(icon));
/// @endcode
/// Every type here is trivially copyable and standard-layout: a loaded struct copies, compares and saves like
/// any plain struct. Nothing here allocates.

#include <cstddef>
#include <cstdint>
#include <limits>

#include "nnh/assert.hpp"
#include "nnh/math.hpp"

namespace nnh {

struct Color; // <nnh/gfx.hpp>; a data struct with a Color member includes it

namespace data {

inline constexpr int kMaxStringBytes = 255;               ///< capacity limit of String<N>
inline constexpr int kMaxListItems = 256;                 ///< capacity limit of List<T, N>
inline constexpr int kMaxFields = 64;                     ///< fields per type's schema
inline constexpr int kMaxDepth = 4;                       ///< nested struct levels below a data type
inline constexpr std::size_t kMaxStructBytes = 16 * 1024; ///< sizeof a data type

/// UTF-8 text of up to N bytes (1 <= N <= 255), always NUL-terminated. Trivially copyable; the bytes after
/// the text are always zero, so equal strings are equal bytes. In a data file: `name = "Goblin"`.
template <int N> class String {
    static_assert(N >= 1 && N <= kMaxStringBytes, "data::String<N> holds 1 to 255 bytes");

public:
    const char* c_str() const { return text_; }
    /// Length in bytes (not characters).
    int size() const {
        int n = 0;
        while (n < N && text_[n] != '\0') ++n;
        return n;
    }
    bool empty() const { return text_[0] == '\0'; }

    /// Copies `text` (null = ""). Returns false if it is longer than N bytes: the copy is then cut at the
    /// last whole UTF-8 character that fits. Clears the rest of the buffer.
    bool assign(const char* text) {
        int n = 0;
        if (text != nullptr) {
            while (n < N && text[n] != '\0') ++n;
        }
        const bool fits = text == nullptr || text[n] == '\0';
        if (!fits) {
            // Cut before a character that would be split: back over continuation bytes to its lead byte.
            int lead = n;
            while (lead > 0 && (static_cast<unsigned char>(text[lead]) & 0xC0) == 0x80) --lead;
            n = lead;
        }
        for (int i = 0; i <= N; ++i) text_[i] = i < n ? text[i] : '\0';
        return fits;
    }

    friend bool operator==(const String& a, const String& b) { return a == b.text_; }
    friend bool operator==(const String& a, const char* b) {
        int i = 0;
        for (; i < N && a.text_[i] != '\0'; ++i) {
            if (a.text_[i] != b[i]) return false;
        }
        return b[i] == '\0';
    }

private:
    char text_[N + 1] = {};
};

/// 0..N values of T (1 <= N <= 256), the count kept; unused slots hold T's default. In a data file: an inline
/// array `[1, 2, 3]`, or one `[[field]]` block per item for structs.
template <class T, int N> class List {
    static_assert(N >= 1 && N <= kMaxListItems, "data::List<T, N> holds 1 to 256 items");

public:
    int size() const { return count_; }
    bool empty() const { return count_ == 0; }
    static constexpr int capacity() { return N; }

    /// Item `i` (0 <= i < size(); checked by NNH_ASSERT).
    T& operator[](int i) {
        NNH_ASSERT(i >= 0 && i < count_, "data::List index %d out of range (%d)", i, count_);
        return items_[i];
    }
    const T& operator[](int i) const {
        NNH_ASSERT(i >= 0 && i < count_, "data::List index %d out of range (%d)", i, count_);
        return items_[i];
    }
    T* begin() { return items_; }
    T* end() { return items_ + count_; }
    const T* begin() const { return items_; }
    const T* end() const { return items_ + count_; }

    /// Appends `value`; false (and nothing changes) when the list is full.
    bool push_back(const T& value) {
        if (count_ >= N) return false;
        items_[count_++] = value;
        return true;
    }
    /// Empties the list and gives every slot T's default back.
    void clear() {
        for (int i = 0; i < count_; ++i) items_[i] = T{};
        count_ = 0;
    }

    /// Where the items start inside a List (the count is at offset 0). For the schema tables.
    static constexpr std::size_t items_offset() { return offsetof(List, items_); }

private:
    std::uint16_t count_ = 0;
    T items_[N] = {};
};

/// A reference to another data asset of type T (which needs NNH_DATA_TYPE): a 32-bit hash of its path, 0 =
/// none. Never followed by itself. In a data file: `drop = "items/potion"` (path without extension), `""` for
/// none.
template <class T> struct Ref {
    std::uint32_t id = 0;
    bool empty() const { return id == 0; }
    friend bool operator==(Ref, Ref) = default;
};

/// A reference to a texture asset, by the path a game passes to load_texture() (`icon = "art/potion"`).
struct TextureRef {
    std::uint32_t id = 0;
    bool empty() const { return id == 0; }
    friend bool operator==(TextureRef, TextureRef) = default;
};

/// A reference to a sound asset, by the path a game passes to load_sound().
struct SoundRef {
    std::uint32_t id = 0;
    bool empty() const { return id == 0; }
    friend bool operator==(SoundRef, SoundRef) = default;
};

/// A reference to a music asset, by the path a game passes to music::play().
struct MusicRef {
    std::uint32_t id = 0;
    bool empty() const { return id == 0; }
    friend bool operator==(MusicRef, MusicRef) = default;
};

/// Rules of one field, written as designated initializers in this order inside NNH_DATA_FIELD:
/// `NNH_DATA_FIELD(hp, .min = 1, .max = 9999, .required = true)`.
struct FieldOptions {
    /// Smallest value a data file may set. Numbers only (a compile error on other kinds); whole numbers for
    /// integer fields. Clamped to the member's type: `.min = -5` on a std::uint8_t means 0.
    double min = std::numeric_limits<double>::lowest();
    /// Largest value a data file may set (same rules as min).
    double max = std::numeric_limits<double>::max();
    /// The file must set the field (a nested struct: must write its section). Otherwise an unset field keeps
    /// the struct's default member initializer.
    bool required = false;
};

} // namespace data
} // namespace nnh

#include "nnh/detail/data_schema.hpp"
