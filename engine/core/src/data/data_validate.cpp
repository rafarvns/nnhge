// Bounded validation of a converted data asset (data_validate.hpp). Pure.

#include "data/data_validate.hpp"

#include <cmath>
#include <cstring>

#include "core/crc32.hpp"
#include "data/byte_reader.hpp"
#include "data/data_format.hpp"
#include "text/utf8.hpp"

namespace nnh::detail {

namespace {

constexpr const char* kTruncated = "truncated";
constexpr const char* kIncomplete = "the type's schema is incomplete";

struct Walk {
    ByteReader in;
    const DataRegistry& registry;
    const char* reason = nullptr; ///< the first problem

    bool fail(const char* why) {
        if (reason == nullptr) reason = why;
        return false;
    }
    /// After a read: false (truncated) if it went past the end.
    bool read_ok() { return !in.failed || fail(kTruncated); }
};

bool struct_value(Walk& w, const TypeInfo& t, int level);

/// Reads a `width`-byte integer, sign-extended when the field is signed.
double int_value(ByteReader& in, const FieldInfo& f) {
    const std::uint32_t raw = in.le(f.width);
    if (!f.is_signed) return static_cast<double>(raw);
    if (f.width >= 4) return static_cast<double>(static_cast<std::int32_t>(raw));
    const std::uint32_t sign = 1u << (8 * f.width - 1);
    return static_cast<double>(static_cast<std::int32_t>((raw ^ sign) - sign));
}

bool float_value(Walk& w, const FieldInfo& f, bool ranged) {
    const std::uint32_t bits = w.in.u32();
    if (!w.read_ok()) return false;
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!std::isfinite(v) || (ranged && (v < f.min || v > f.max))) return w.fail("float out of range");
    return true;
}

bool string_value(Walk& w, const FieldInfo& f) {
    const std::uint32_t length = w.in.u8();
    if (!w.read_ok()) return false;
    if (length > f.capacity) return w.fail("string longer than its capacity");
    const auto* text = reinterpret_cast<const char*>(w.in.take(length));
    if (!w.read_ok()) return false;
    if (std::memchr(text, '\0', length) != nullptr) return w.fail("string with a NUL byte");
    if (!utf8_valid(text, length)) return w.fail("string isn't UTF-8");
    return true;
}

bool element(Walk& w, const FieldInfo& f, int level) {
    switch (f.kind) {
    case FieldKind::Bool: {
        const std::uint32_t v = w.in.u8();
        if (!w.read_ok()) return false;
        return v <= 1 || w.fail("bool is neither 0 nor 1");
    }
    case FieldKind::Int: {
        if (f.width != 1 && f.width != 2 && f.width != 4) return w.fail(kIncomplete);
        const double v = int_value(w.in, f);
        if (!w.read_ok()) return false;
        return (v >= f.min && v <= f.max) || w.fail("integer out of range");
    }
    case FieldKind::Float:
        return float_value(w, f, true);
    case FieldKind::Vec2:
        return float_value(w, f, false) && float_value(w, f, false);
    case FieldKind::String:
        return string_value(w, f);
    case FieldKind::Enum: {
        const std::uint32_t index = w.in.u8();
        if (!w.read_ok()) return false;
        const EnumInfo* e = find_enum(w.registry, f.target);
        if (e == nullptr) return w.fail(kIncomplete);
        return index < e->count || w.fail("enum index out of range");
    }
    case FieldKind::Color:
    case FieldKind::DataRef:
    case FieldKind::TextureRef:
    case FieldKind::SoundRef:
    case FieldKind::MusicRef:
        (void)w.in.take(4);
        return w.read_ok();
    case FieldKind::Struct: {
        const TypeInfo* nested = find_type(w.registry, f.target);
        if (nested == nullptr) return w.fail(kIncomplete);
        if (level + 1 > data::kMaxDepth) return w.fail("structs nested too deep");
        return struct_value(w, *nested, level + 1);
    }
    }
    return w.fail(kIncomplete);
}

bool value(Walk& w, const FieldInfo& f, int level) {
    int count = f.count;
    if (f.shape == FieldShape::One) {
        count = 1;
    } else if (f.shape == FieldShape::List) {
        count = static_cast<int>(w.in.u16());
        if (!w.read_ok()) return false;
        if (count > f.count) return w.fail("list longer than its capacity");
    }
    for (int i = 0; i < count; ++i) {
        if (!element(w, f, level)) return false;
    }
    return true;
}

bool struct_value(Walk& w, const TypeInfo& t, int level) {
    if (t.field_count > data::kMaxFields) return w.fail(kIncomplete);
    const std::uint32_t n = w.in.u8();
    if (!w.read_ok()) return false;
    constexpr const char* kBadIndex = "field indices out of order or out of range";
    if (n > t.field_count) return w.fail(kBadIndex);
    std::uint64_t seen = 0;
    int previous = -1;
    for (std::uint32_t k = 0; k < n; ++k) {
        const int index = static_cast<int>(w.in.u8());
        if (!w.read_ok()) return false;
        if (index <= previous || index >= t.field_count) return w.fail(kBadIndex);
        previous = index;
        seen |= std::uint64_t{1} << index;
        if (!value(w, t.fields[index], level)) return false;
    }
    for (int i = 0; i < t.field_count; ++i) {
        if (t.fields[i].required && (seen & (std::uint64_t{1} << i)) == 0) {
            return w.fail("a required field is missing");
        }
    }
    return true;
}

std::uint32_t header_u32(const std::uint8_t* p, std::size_t at) {
    return p[at] | (p[at + 1] << 8) | (p[at + 2] << 16) | (static_cast<std::uint32_t>(p[at + 3]) << 24);
}

DataCheck problem(DataProblem p, const char* reason) { return {p, reason}; }

} // namespace

DataCheck validate_data(const void* data, std::size_t size, const DataRegistry& registry,
                        const TypeInfo& type, std::uint32_t fingerprint) {
    using namespace datafmt;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (size < kHeaderSize) return problem(DataProblem::Damaged, "too short to be a data asset");
    const std::uint32_t version = bytes[kVersionAt] | (bytes[kVersionAt + 1] << 8);
    if (std::memcmp(bytes, kMagic, 4) != 0 || version == 0) {
        return problem(DataProblem::Damaged, "not a converted data asset");
    }
    if (version > kVersion) return problem(DataProblem::NewerVersion, "converted by a newer NNHGE");
    if ((bytes[kFlagsAt] | bytes[kFlagsAt + 1]) != 0) {
        return problem(DataProblem::NewerVersion, "uses a feature this engine doesn't read");
    }
    if (header_u32(bytes, kSizeAt) != size)
        return problem(DataProblem::Damaged, "size doesn't match the header");
    if (size > kMaxFileBytes) return problem(DataProblem::OverLimit, "larger than 64 KiB");
    if (header_u32(bytes, kCrcAt) != crc32(bytes + kHeaderSize, size - kHeaderSize)) {
        return problem(DataProblem::Damaged, "CRC mismatch");
    }
    if (header_u32(bytes, kFingerprintAt) != fingerprint) {
        return problem(DataProblem::Stale, "converted for another version of the type: rebuild the assets");
    }
    Walk w{ByteReader{bytes + kHeaderSize, size - kHeaderSize}, registry};
    if (!struct_value(w, type, 0)) return problem(DataProblem::Damaged, w.reason);
    if (w.in.at != w.in.size) return problem(DataProblem::Damaged, "bytes after the end of the value");
    return {};
}

} // namespace nnh::detail
