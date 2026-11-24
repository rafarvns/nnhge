// detail::save_encode (spec 027, D45): a value's fields in ascending id order, sequences packed, struct
// sequences as one template and rows. Every write checks the room left first, so nothing is written past the
// buffer. A length prefix is written after its content (one byte reserved, the content moved when the length
// needs more), so each value is encoded once.

#include "nnh/saves.hpp"
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

    bool room(std::size_t n) {
        if (static_cast<std::size_t>(end - p) < n) full = true;
        return !full;
    }
    bool byte(std::uint8_t b) {
        if (!room(1)) return false;
        *p++ = b;
        return true;
    }
    bool bytes(const void* data, std::size_t n) {
        if (!room(n)) return false;
        if (n > 0) std::memcpy(p, data, n);
        p += n;
        return true;
    }
    bool varint(std::uint32_t v) {
        if (!room(static_cast<std::size_t>(varint_size(v)))) return false;
        p += put_varint(p, v);
        return true;
    }
    /// Writes the length of [start, p) at `at` (one byte reserved there), moving the content when it needs
    /// more.
    bool close_length(std::uint8_t* at, std::uint8_t* start) {
        const std::uint32_t length = static_cast<std::uint32_t>(p - start);
        const int extra = varint_size(length) - 1;
        if (extra > 0) {
            if (!room(static_cast<std::size_t>(extra))) return false;
            std::memmove(start + extra, start, length);
            p += extra;
        }
        put_varint(at, length);
        return true;
    }
};

bool put_fields(Out& out, const SaveTypeInfo& type, const std::uint8_t* base);
bool put_value(Out& out, const SaveFieldInfo& f, const std::uint8_t* member);

bool put_string(Out& out, const std::uint8_t* text, int capacity) {
    int n = 0;
    while (n < capacity && text[n] != 0)
        ++n;
    if (!utf8_valid(reinterpret_cast<const char*>(text), static_cast<std::size_t>(n))) {
        if (!out.bad_text) out.bad_at = out.path;
        out.bad_text = true;
    }
    return out.byte(static_cast<std::uint8_t>(n)) && out.bytes(text, static_cast<std::size_t>(n));
}

bool put_struct(Out& out, const SaveTypeInfo& nested, const std::uint8_t* src) {
    std::uint8_t* at = out.p;
    if (!out.byte(0)) return false;
    std::uint8_t* start = out.p;
    return put_fields(out, nested, src) && out.close_length(at, start);
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
        return out.bytes(src, static_cast<std::size_t>(wire_width(f.kind)));
    }
}

/// A struct sequence: the item struct's template (its fields' wire bytes and ids), then `bytes`, then the
/// rows (each item's values in template order, without tags).
bool put_rows(Out& out, const SaveTypeInfo& nested, const std::uint8_t* items, int count,
              std::uint32_t stride) {
    if (!out.varint(nested.field_count)) return false;
    for (int i = 0; i < nested.field_count; ++i) {
        const SaveFieldInfo& f = nested.fields[i];
        if (!out.byte(wire_byte(f.kind, f.shape)) || !out.varint(f.id)) return false;
    }
    std::uint8_t* at = out.p;
    if (!out.byte(0)) return false;
    std::uint8_t* start = out.p;
    for (int item = 0; item < count; ++item) {
        out.path.push(static_cast<std::uint16_t>(item), true);
        const std::uint8_t* row = items + static_cast<std::size_t>(item) * stride;
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
        for (int i = 0; i < count; ++i)
            *out.p++ = items[i] != 0 ? 1 : 0;
        return true;
    }
    if (f.kind != WireKind::String) { // fixed widths: one packed copy (stride == wire width, little-endian)
        return out.bytes(items, static_cast<std::size_t>(count) * f.stride);
    }
    std::uint8_t* at = out.p;
    if (!out.byte(0)) return false;
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
