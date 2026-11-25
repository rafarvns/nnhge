// detail::save_encode (spec 027, D45): a value's fields in ascending id order, sequences packed, struct
// sequences as one template and rows. Every write checks the room left first, so nothing is written past the
// buffer. A length prefix is written after its content (the varint size of the content's smallest possible
// length reserved, the content moved when the length needs more), so each value is encoded once. Rows of
// single fixed values and strings are written without per-value checks or path steps when the struct's
// largest row fits (spec 027 T10.3).

#include "nnh/saves.hpp"
#include "saves/save_bulk.hpp"
#include "saves/save_header.hpp"
#include "saves/save_wire.hpp"
#include "text/utf8.hpp"

#include <cstring>

namespace nnh::detail {
namespace {

using save::WireKind;
using save::WireShape;

struct Out {
    std::uint8_t* p = nullptr;
    std::uint8_t* end = nullptr;
    bool full = false;
    bool bad_text = false;
    save::FieldPath path; ///< where the encoder is (for the bad-text message)
    save::FieldPath bad_at;

    /// Whether `n` more bytes fit (nothing recorded).
    bool has(std::size_t n) const { return static_cast<std::size_t>(end - p) >= n; }
    bool room(std::size_t n) {
        if (!has(n)) full = true;
        return !full;
    }
    bool byte(std::uint8_t b) {
        if (!room(1)) return false;
        *p++ = b;
        return true;
    }
    bool bytes(const std::uint8_t* data, std::size_t n) {
        if (!room(n)) return false;
        copy_bytes(p, data, n);
        p += n;
        return true;
    }
    /// One value of a fixed kind: 1, 2, 4 or 8 bytes.
    bool fixed(const std::uint8_t* src, int width) {
        if (!room(static_cast<std::size_t>(width))) return false;
        copy_value(p, src, width);
        p += width;
        return true;
    }
    bool varint(std::uint32_t v) {
        if (!room(static_cast<std::size_t>(varint_size(v)))) return false;
        p += put_varint(p, v);
        return true;
    }
    /// Reserves the length prefix of content at least `least` bytes long: its varint's size, so the content
    /// moves only when it turns out longer. Returns where the prefix goes (the content starts after it), or
    /// nullptr when full.
    std::uint8_t* open_length(std::uint32_t least) {
        const int n = varint_size(least < kMaxVarint ? least : kMaxVarint);
        if (!room(static_cast<std::size_t>(n))) return nullptr;
        std::uint8_t* at = p;
        p += n;
        return at;
    }
    /// Writes the length of [start, p) at `at` (start - at bytes reserved there), moving the content when its
    /// varint needs another size.
    bool close_length(std::uint8_t* at, std::uint8_t* start) {
        const std::uint32_t length = static_cast<std::uint32_t>(p - start);
        const std::ptrdiff_t shift = varint_size(length) - (start - at);
        if (shift > 0 && !room(static_cast<std::size_t>(shift))) return false;
        if (shift != 0) {
            std::memmove(start + shift, start, length);
            p += shift;
        }
        put_varint(at, length);
        return true;
    }
    /// Records the first string that isn't UTF-8 at the current path followed by row `item` and field `id`.
    void bad_string_at(std::uint16_t item, std::uint16_t id) {
        if (bad_text) return;
        bad_text = true;
        bad_at = path;
        bad_at.push(item, true);
        bad_at.push(id, false);
    }
};

/// Bytes of the string at `text` (a data::String of `capacity`).
int string_length(const std::uint8_t* text, int capacity) {
    int n = 0;
    while (n < capacity && text[n] != 0)
        ++n;
    return n;
}

bool put_fields(Out& out, const SaveTypeInfo& type, const std::uint8_t* base);
bool put_value(Out& out, const SaveFieldInfo& f, const std::uint8_t* member);

bool text_ok(const std::uint8_t* text, int n) {
    return utf8_valid(reinterpret_cast<const char*>(text), static_cast<std::size_t>(n));
}

bool put_string(Out& out, const std::uint8_t* text, int capacity) {
    const int n = string_length(text, capacity);
    if (!text_ok(text, n)) {
        if (!out.bad_text) out.bad_at = out.path;
        out.bad_text = true;
    }
    return out.byte(static_cast<std::uint8_t>(n)) && out.bytes(text, static_cast<std::size_t>(n));
}

bool put_struct(Out& out, const SaveTypeInfo& nested, const std::uint8_t* src) {
    std::uint8_t* at = out.open_length(0);
    return at != nullptr && put_fields(out, nested, src) && out.close_length(at, at + 1);
}

bool put_one(Out& out, const SaveFieldInfo& f, const std::uint8_t* src) {
    switch (f.kind) {
    case WireKind::Bool:
        return out.byte(*src != 0 ? 1 : 0);
    case WireKind::String:
        return put_string(out, src, f.capacity);
    case WireKind::Struct:
        return put_struct(out, *f.nested, src);
    default:
        return out.fixed(src, wire_width(f.kind));
    }
}

/// One row whose columns are all single fixed values and strings, written without room checks (the caller
/// made sure the struct's largest row fits) and without path steps.
void put_flat_row(Out& out, const SaveTypeInfo& nested, const std::uint8_t* row, int item) {
    std::uint8_t* p = out.p;
    for (int i = 0; i < nested.field_count; ++i) {
        const SaveFieldInfo& f = nested.fields[i];
        const std::uint8_t* src = row + f.offset;
        if (f.kind == WireKind::String) {
            const int n = string_length(src, f.capacity);
            if (!text_ok(src, n)) out.bad_string_at(static_cast<std::uint16_t>(item), f.id);
            *p++ = static_cast<std::uint8_t>(n);
            copy_bytes(p, src, static_cast<std::size_t>(n));
            p += n;
        } else if (f.kind == WireKind::Bool) {
            *p++ = *src != 0 ? 1 : 0;
        } else {
            const int width = wire_width(f.kind);
            copy_value(p, src, width);
            p += width;
        }
    }
    out.p = p;
}

/// A struct sequence: the item struct's template (its fields' wire bytes and ids), then `bytes`, then the
/// rows (each item's values in template order, without tags).
bool put_rows(Out& out, const SaveTypeInfo& nested, const std::uint8_t* items, int count,
              std::uint32_t stride) {
    if (!out.varint(nested.field_count)) return false;
    std::uint32_t least = 0; // the smallest row: the fixed widths, a byte for anything else
    bool flat = true;        // every column one fixed value or string
    for (int i = 0; i < nested.field_count; ++i) {
        const SaveFieldInfo& f = nested.fields[i];
        if (!out.byte(wire_byte(f.kind, f.shape)) || !out.varint(f.id)) return false;
        const bool one = f.shape == WireShape::One;
        const int width = wire_width(f.kind);
        least += one && width > 0 ? static_cast<std::uint32_t>(width) : 1u;
        flat = flat && one && f.kind != WireKind::Struct;
    }
    std::uint8_t* at = out.open_length(least * static_cast<std::uint32_t>(count));
    if (at == nullptr) return false;
    std::uint8_t* start = out.p;
    for (int item = 0; item < count; ++item) {
        const std::uint8_t* row = items + static_cast<std::size_t>(item) * stride;
        if (flat && out.has(nested.max_row)) {
            put_flat_row(out, nested, row, item);
            continue;
        }
        out.path.push(static_cast<std::uint16_t>(item), true);
        for (int i = 0; i < nested.field_count; ++i) {
            const SaveFieldInfo& f = nested.fields[i];
            out.path.push(f.id, false);
            if (!put_value(out, f, row + f.offset)) return false;
            out.path.pop();
        }
        out.path.pop();
    }
    return out.close_length(at, start);
}

bool put_sequence(Out& out, const SaveFieldInfo& f, const std::uint8_t* member) {
    int count = f.count;
    const std::uint8_t* items = member;
    if (f.shape == WireShape::List) {
        count = load_u16(member);
        if (count > f.count) count = f.count; // a List's count never exceeds its capacity; never trust memory
        items = member + f.items_offset;
    }
    if (!out.varint(static_cast<std::uint32_t>(count))) return false;
    if (f.kind == WireKind::Struct) return put_rows(out, *f.nested, items, count, f.stride);
    if (f.kind == WireKind::Bool) {
        if (!out.room(static_cast<std::size_t>(count))) return false;
        put_bools(out.p, items, static_cast<std::size_t>(count));
        out.p += count;
        return true;
    }
    if (f.kind != WireKind::String) { // fixed widths: one packed copy (stride == wire width, little-endian)
        return out.bytes(items, static_cast<std::size_t>(count) * f.stride);
    }
    std::uint8_t* at = out.open_length(static_cast<std::uint32_t>(count)); // a byte per string at least
    if (at == nullptr) return false;
    std::uint8_t* start = out.p;
    for (int i = 0; i < count; ++i) {
        out.path.push(static_cast<std::uint16_t>(i), true);
        if (!put_string(out, items + static_cast<std::size_t>(i) * f.stride, f.capacity)) return false;
        out.path.pop();
    }
    return out.close_length(at, start);
}

bool put_value(Out& out, const SaveFieldInfo& f, const std::uint8_t* member) {
    return f.shape == WireShape::One ? put_one(out, f, member) : put_sequence(out, f, member);
}

bool put_fields(Out& out, const SaveTypeInfo& type, const std::uint8_t* base) {
    for (int i = 0; i < type.field_count; ++i) {
        const SaveFieldInfo& f = type.fields[i];
        if (!out.byte(wire_byte(f.kind, f.shape)) || !out.varint(f.id)) return false;
        out.path.push(f.id, false);
        if (!put_value(out, f, base + f.offset)) return false;
        out.path.pop();
    }
    return true;
}

} // namespace

Result<std::size_t> save_encode(const SaveTypeInfo& type, const void* value, void* out,
                                std::size_t capacity) {
    Out o;
    o.p = static_cast<std::uint8_t*>(out);
    o.end = o.p + capacity;
    const std::size_t header = kSaveStructHeaderSize + std::strlen(type.key);
    if (o.room(header)) {
        o.p += build_save_struct_header(o.p, type.key, type.version);
        put_fields(o, type, static_cast<const std::uint8_t*>(value));
    }
    if (o.full) {
        return Error::make(ErrorCode::LimitReached, "save buffer of %lu bytes too small (up to %lu needed)",
                           static_cast<unsigned long>(capacity),
                           static_cast<unsigned long>(header + type.max_fields));
    }
    if (o.bad_text) {
        char where[48];
        o.bad_at.format(where, sizeof where);
        NNH_ASSERT(false, "save '%s': field %s: string is not UTF-8", type.key, where);
        return Error::make(ErrorCode::InvalidArgument, "field %s: string is not UTF-8", where);
    }
    return static_cast<std::size_t>(o.p - static_cast<std::uint8_t*>(out));
}

} // namespace nnh::detail
