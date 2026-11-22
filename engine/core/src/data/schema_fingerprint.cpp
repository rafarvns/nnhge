// Schema fingerprint (schema_fingerprint.hpp). Pure.

#include "data/schema_fingerprint.hpp"

#include <cstring>

#include "core/crc32.hpp"
#include "data/data_format.hpp"
#include "data/enum_names.hpp"

namespace nnh::detail {

namespace {

// Markers in place of a nested type that can't be encoded.
constexpr std::uint8_t kUnknownType = 0xFE;
constexpr std::uint8_t kTooDeep = 0xFF;

/// Fingerprints of the nested types already encoded during one call (each type is encoded once).
struct Memo {
    const void* tags[datafmt::kMaxTypes];
    std::uint32_t crcs[datafmt::kMaxTypes];
    int count = 0;
};

struct Encoder {
    std::uint32_t crc = 0;

    void bytes(const void* p, std::size_t n) { crc = crc32_update(crc, p, n); }
    void u8(std::uint32_t v) {
        const std::uint8_t b = static_cast<std::uint8_t>(v);
        bytes(&b, 1);
    }
    void u16(std::uint32_t v) {
        u8(v);
        u8(v >> 8);
    }
    void u32(std::uint32_t v) {
        u16(v);
        u16(v >> 16);
    }
    void f64(double v) {
        std::uint64_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        u32(static_cast<std::uint32_t>(bits));
        u32(static_cast<std::uint32_t>(bits >> 32));
    }
    void text(const char* s, std::size_t n) {
        u16(static_cast<std::uint32_t>(n)); // names are at most 63 bytes (check_schema)
        bytes(s, n);
    }
    void text(const char* s) { text(s, std::strlen(s)); }
};

std::uint32_t encode_type(const DataRegistry& r, const TypeInfo& t, int level, Memo& memo);

void encode_enum(Encoder& e, const EnumInfo* info) {
    if (info == nullptr) return e.u8(kUnknownType);
    e.u16(info->count);
    for (int i = 0; i < info->count; ++i) {
        const NameView v = enum_name(info->names, i);
        e.text(v.text, static_cast<std::size_t>(v.length));
    }
}

void encode_nested(Encoder& e, const DataRegistry& r, const FieldInfo& f, int level, Memo& memo) {
    const TypeInfo* nested = find_type(r, f.target);
    if (nested == nullptr) return e.u8(kUnknownType);
    if (level >= data::kMaxDepth + 1) return e.u8(kTooDeep);
    for (int i = 0; i < memo.count; ++i) {
        if (memo.tags[i] == f.target) return e.u32(memo.crcs[i]);
    }
    const std::uint32_t crc = encode_type(r, *nested, level + 1, memo);
    if (memo.count < datafmt::kMaxTypes) {
        memo.tags[memo.count] = f.target;
        memo.crcs[memo.count++] = crc;
    }
    e.u32(crc);
}

void encode_field(Encoder& e, const DataRegistry& r, const FieldInfo& f, int level, Memo& memo) {
    e.text(f.name);
    e.u8(static_cast<std::uint8_t>(f.kind));
    e.u8(static_cast<std::uint8_t>(f.shape));
    e.u16(f.count);
    e.u16(f.capacity);
    if (f.kind == FieldKind::Int) {
        e.u8(f.width);
        e.u8(f.is_signed ? 1 : 0);
    }
    if (f.kind == FieldKind::Int || f.kind == FieldKind::Float) {
        e.f64(f.min);
        e.f64(f.max);
    }
    e.u8(f.required ? 1 : 0);
    if (f.kind == FieldKind::Struct) {
        encode_nested(e, r, f, level, memo);
    } else if (f.kind == FieldKind::Enum) {
        encode_enum(e, find_enum(r, f.target));
    } else if (f.kind == FieldKind::DataRef) {
        const TypeInfo* target = find_type(r, f.target);
        if (target == nullptr) {
            e.u8(kUnknownType);
        } else {
            e.text(target->key);
        }
    }
}

std::uint32_t encode_type(const DataRegistry& r, const TypeInfo& t, int level, Memo& memo) {
    Encoder e;
    e.bytes("NNHD-schema", 11); // format of this encoding: change the text if the encoding changes
    e.text(t.key);
    e.u16(t.field_count);
    for (int i = 0; i < t.field_count; ++i) encode_field(e, r, t.fields[i], level, memo);
    return e.crc;
}

} // namespace

std::uint32_t schema_fingerprint(const DataRegistry& registry, const TypeInfo& type) {
    Memo memo;
    return encode_type(registry, type, 0, memo);
}

} // namespace nnh::detail
