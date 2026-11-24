// The schema-free structural check of a structured save's fields (save_validate.hpp). A recursive walk
// bounded by the 8 struct levels; every length and count is checked against the bytes left before it is used.

#include "saves/save_validate.hpp"

#include "saves/save_wire.hpp"
#include "text/utf8.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace nnh::detail {
namespace {

using save::WireKind;
using save::WireShape;

struct Walk {
    std::uint32_t steps = 0;
    save::FieldPath path;
    Error error{};

    /// Records the first fault: "field <path>: <reason>" (the path ends in "?" when the id itself is the
    /// fault).
    bool fail(bool id_unknown, const char* format, ...) NNH_PRINTF_FORMAT(3, 4) {
        char reason[48];
        va_list args;
        va_start(args, format);
        std::vsnprintf(reason, sizeof reason, format, args);
        va_end(args);
        char where[64];
        const int n = path.format(where, sizeof where);
        if (id_unknown)
            std::snprintf(where + n, sizeof where - static_cast<std::size_t>(n), "%s?", n > 0 ? "." : "");
        error = Error::make(ErrorCode::Corrupted, "field %s: %s", where, reason);
        return false;
    }

    bool string(const std::uint8_t*& p, const std::uint8_t* end) {
        if (p >= end || end - (p + 1) < static_cast<std::ptrdiff_t>(*p))
            return fail(false, "string cut short");
        const std::size_t length = *p++;
        const char* text = reinterpret_cast<const char*>(p);
        if (!utf8_valid(text, length)) return fail(false, "string is not UTF-8");
        if (length > 0 && std::memchr(text, 0, length) != nullptr) return fail(false, "string holds a NUL");
        p += length;
        return true;
    }

    bool length(const std::uint8_t*& p, const std::uint8_t* end, std::uint32_t& out) {
        const int n = get_varint(p, end, out);
        if (n == 0) return fail(false, "bad length");
        p += n;
        return true;
    }

    bool fields(const std::uint8_t*& p, const std::uint8_t* end, int depth);
    bool value(std::uint8_t wire, const std::uint8_t*& p, const std::uint8_t* end, int depth);
    bool one(WireKind kind, const std::uint8_t*& p, const std::uint8_t* end, int depth);
    bool sequence(WireKind kind, const std::uint8_t*& p, const std::uint8_t* end, int depth);
    bool strings(std::uint32_t count, const std::uint8_t*& p, const std::uint8_t* end);
    bool rows(std::uint32_t count, const std::uint8_t*& p, const std::uint8_t* end, int depth);
};

bool Walk::fields(const std::uint8_t*& p, const std::uint8_t* end, int depth) {
    std::uint32_t previous = 0;
    while (p < end) {
#if defined(__GNUC__)
        // A sequential walk: ask for the next lines (pld on the 3DS, which has no prefetcher).
        __builtin_prefetch(p + 64);
#endif
        const std::uint8_t wire = *p++;
        if (!wire_valid(wire)) return fail(true, "bad wire byte 0x%02X", static_cast<unsigned>(wire));
        std::uint32_t id = 0;
        const int n = get_varint(p, end, id);
        if (n == 0 || id == 0 || id > static_cast<std::uint32_t>(save::kMaxId)) return fail(true, "bad id");
        p += n;
        path.push(static_cast<std::uint16_t>(id), false);
        if (id <= previous) return fail(false, "id not ascending");
        if (!value(wire, p, end, depth)) return false;
        path.pop();
        previous = id;
    }
    return true;
}

bool Walk::value(std::uint8_t wire, const std::uint8_t*& p, const std::uint8_t* end, int depth) {
    ++steps;
    const WireKind kind = wire_kind(wire);
    return wire_shape(wire) == WireShape::One ? one(kind, p, end, depth) : sequence(kind, p, end, depth);
}

bool Walk::one(WireKind kind, const std::uint8_t*& p, const std::uint8_t* end, int depth) {
    if (kind == WireKind::String) return string(p, end);
    if (kind != WireKind::Struct) {
        const int width = wire_width(kind);
        if (end - p < width) return fail(false, "value cut short");
        p += width;
        return true;
    }
    std::uint32_t size = 0;
    if (!length(p, end, size)) return false;
    if (static_cast<std::uint32_t>(end - p) < size) return fail(false, "struct length exceeds the file");
    if (depth + 1 > save::kMaxDepth) return fail(false, "nesting deeper than 8 levels");
    const std::uint8_t* inner_end = p + size;
    return fields(p, inner_end, depth + 1);
}

bool Walk::sequence(WireKind kind, const std::uint8_t*& p, const std::uint8_t* end, int depth) {
    std::uint32_t count = 0;
    const int n = get_varint(p, end, count);
    if (n == 0 || count > static_cast<std::uint32_t>(save::kMaxArrayItems)) return fail(false, "bad count");
    p += n;
    if (kind == WireKind::String) return strings(count, p, end);
    if (kind == WireKind::Struct) return rows(count, p, end, depth);
    const std::uint64_t bytes =
        static_cast<std::uint64_t>(count) * static_cast<std::uint64_t>(wire_width(kind));
    if (static_cast<std::uint64_t>(end - p) < bytes) {
        return fail(false, "list count %lu exceeds the file", static_cast<unsigned long>(count));
    }
    p += bytes;
    return true;
}

bool Walk::strings(std::uint32_t count, const std::uint8_t*& p, const std::uint8_t* end) {
    std::uint32_t bytes = 0;
    if (!length(p, end, bytes)) return false;
    if (static_cast<std::uint32_t>(end - p) < bytes)
        return fail(false, "length %lu exceeds the file", static_cast<unsigned long>(bytes));
    if (count > bytes)
        return fail(false, "list count %lu exceeds the file", static_cast<unsigned long>(count));
    const std::uint8_t* region_end = p + bytes;
    for (std::uint32_t i = 0; i < count; ++i) {
        ++steps;
        path.push(static_cast<std::uint16_t>(i), true);
        if (!string(p, region_end)) return false;
        path.pop();
    }
    if (p != region_end) return fail(false, "items don't fill their length");
    return true;
}

bool Walk::rows(std::uint32_t count, const std::uint8_t*& p, const std::uint8_t* end, int depth) {
    std::uint32_t fields_in_row = 0;
    const int n = get_varint(p, end, fields_in_row);
    if (n == 0) return fail(false, "template: bad size");
    p += n;
    if (fields_in_row == 0 || fields_in_row > static_cast<std::uint32_t>(save::kMaxFields)) {
        return fail(false, "template of %lu fields", static_cast<unsigned long>(fields_in_row));
    }
    const std::uint8_t* row_template = p;
    std::uint32_t previous = 0;
    for (std::uint32_t i = 0; i < fields_in_row; ++i) {
        ++steps;
        if (p >= end) return fail(false, "template cut short");
        if (!wire_valid(*p)) return fail(false, "template: bad wire byte 0x%02X", static_cast<unsigned>(*p));
        ++p;
        std::uint32_t id = 0;
        const int m = get_varint(p, end, id);
        if (m == 0 || id == 0 || id > static_cast<std::uint32_t>(save::kMaxId))
            return fail(false, "template: bad id");
        if (id <= previous) return fail(false, "template ids not ascending");
        p += m;
        previous = id;
    }
    std::uint32_t bytes = 0;
    if (!length(p, end, bytes)) return false;
    if (static_cast<std::uint32_t>(end - p) < bytes)
        return fail(false, "length %lu exceeds the file", static_cast<unsigned long>(bytes));
    if (static_cast<std::uint64_t>(count) * fields_in_row > bytes) {
        return fail(false, "list count %lu exceeds the file", static_cast<unsigned long>(count));
    }
    if (count > 0 && depth + 1 > save::kMaxDepth) return fail(false, "nesting deeper than 8 levels");
    const std::uint8_t* region_end = p + bytes;
    for (std::uint32_t row = 0; row < count; ++row) {
#if defined(__GNUC__)
        __builtin_prefetch(p + 64);
#endif
        path.push(static_cast<std::uint16_t>(row), true);
        const std::uint8_t* t = row_template;
        for (std::uint32_t i = 0; i < fields_in_row; ++i) {
            const std::uint8_t wire = *t++;
            path.push(static_cast<std::uint16_t>(take_varint(t)), false);
            if (!value(wire, p, region_end, depth + 1)) return false;
            path.pop();
        }
        path.pop();
    }
    if (p != region_end) return fail(false, "items don't fill their length");
    return true;
}

} // namespace

Result<void> save_validate_fields(const std::uint8_t* data, std::size_t size, std::uint32_t* steps) {
    Walk walk;
    const std::uint8_t* p = data;
    const bool ok = walk.fields(p, data + size, 1);
    if (steps != nullptr) *steps = walk.steps;
    if (!ok) return walk.error;
    return {};
}

} // namespace nnh::detail
