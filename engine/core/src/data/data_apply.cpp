// Applying a converted data asset to a struct (data_apply.hpp). Pure.

#include "data/data_apply.hpp"

#include <cstdint>
#include <cstring>

#include "data/byte_reader.hpp"
#include "data/data_format.hpp"

namespace nnh::detail {

namespace {

using Byte = unsigned char;

struct Apply {
    ByteReader in;
    const DataRegistry& registry;
};

/// Stores the low `width` bytes of `v` as a native integer of that width (two's complement for signed
/// members).
bool store(Byte* at, std::uint32_t v, int width) {
    if (width == 1) {
        const auto x = static_cast<std::uint8_t>(v);
        std::memcpy(at, &x, 1);
    } else if (width == 2) {
        const auto x = static_cast<std::uint16_t>(v);
        std::memcpy(at, &x, 2);
    } else if (width == 4) {
        std::memcpy(at, &v, 4);
    } else {
        return false;
    }
    return true;
}

bool struct_value(Apply& a, const TypeInfo& t, Byte* base, int level);

bool string_value(Apply& a, const FieldInfo& f, Byte* at) {
    const std::uint32_t length = a.in.u8();
    const std::uint8_t* text = a.in.take(length);
    if (text == nullptr) return false;
    const std::size_t n = length < f.capacity ? length : f.capacity;
    std::memcpy(at, text, n);
    std::memset(at + n, 0, f.capacity + 1u - n); // String<N> holds N + 1 bytes
    return true;
}

bool element(Apply& a, const FieldInfo& f, Byte* at, int level) {
    switch (f.kind) {
    case FieldKind::Bool: {
        const bool v = a.in.u8() != 0;
        std::memcpy(at, &v, 1);
        return !a.in.failed;
    }
    case FieldKind::Int:
        return store(at, a.in.le(f.width), f.width) && !a.in.failed;
    case FieldKind::Float:
    case FieldKind::DataRef:
    case FieldKind::TextureRef:
    case FieldKind::SoundRef:
    case FieldKind::MusicRef:
        return store(at, a.in.u32(), 4) && !a.in.failed;
    case FieldKind::Vec2:
        return store(at, a.in.u32(), 4) && store(at + 4, a.in.u32(), 4) && !a.in.failed;
    case FieldKind::Color: {
        const std::uint8_t* rgba = a.in.take(4);
        if (rgba != nullptr) std::memcpy(at, rgba, 4);
        return rgba != nullptr;
    }
    case FieldKind::String:
        return string_value(a, f, at);
    case FieldKind::Enum: {
        const std::uint32_t index = a.in.u8();
        const EnumInfo* e = find_enum(a.registry, f.target);
        if (a.in.failed || e == nullptr || index >= e->count) return false;
        return store(at, static_cast<std::uint32_t>(e->values[index]), f.width);
    }
    case FieldKind::Struct: {
        const TypeInfo* nested = find_type(a.registry, f.target);
        return nested != nullptr && level < data::kMaxDepth && struct_value(a, *nested, at, level + 1);
    }
    }
    return false;
}

bool value(Apply& a, const FieldInfo& f, Byte* base, int level) {
    Byte* at = base + f.offset;
    if (f.shape == FieldShape::One) return element(a, f, at, level);
    int count = f.count;
    if (f.shape == FieldShape::List) {
        std::uint32_t written = a.in.u16();
        if (a.in.failed) return false;
        written = written < f.count ? written : f.count;
        const auto stored = static_cast<std::uint16_t>(written);
        std::memcpy(at, &stored, 2); // List::count_, at offset 0
        count = static_cast<int>(written);
        at += f.items_offset;
    }
    for (int i = 0; i < count; ++i) {
        if (!element(a, f, at + static_cast<std::size_t>(i) * f.stride, level)) return false;
    }
    return true;
}

bool struct_value(Apply& a, const TypeInfo& t, Byte* base, int level) {
    const std::uint32_t n = a.in.u8();
    for (std::uint32_t k = 0; k < n; ++k) {
        const std::uint32_t index = a.in.u8();
        if (a.in.failed || index >= t.field_count) return false;
        if (!value(a, t.fields[index], base, level)) return false;
    }
    return !a.in.failed;
}

} // namespace

void apply_data(const void* data, std::size_t size, const DataRegistry& registry, const TypeInfo& type,
                void* out) {
    if (size < datafmt::kHeaderSize) return;
    Apply a{ByteReader{static_cast<const std::uint8_t*>(data) + datafmt::kHeaderSize,
                       size - datafmt::kHeaderSize},
            registry};
    (void)struct_value(a, type, static_cast<Byte*>(out), 0);
}

} // namespace nnh::detail
