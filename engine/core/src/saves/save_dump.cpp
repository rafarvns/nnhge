// The text dump of structured saves (spec 027, FR-17): one line per field, `name (id) kind = value`, nested
// structs and items indented, sequences with their count, strings quoted and escaped, floats with their bits;
// ids the schema doesn't know as `#id`, retired ones as `#id (retired)`. Debug and Development only: Release
// compiles stubs that return Unsupported, and no field name is kept there (save_macros.hpp).

#include "fs/save_format.hpp"
#include "nnh/fs.hpp"
#include "saves/save_cursor.hpp"
#include "saves/save_header.hpp"
#include "saves/save_internal.hpp"
#include "saves/save_validate.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace nnh::detail {

#if !defined(NNH_FLAVOR_RELEASE)
namespace {

using save::WireKind;
using save::WireShape;

const char* const kKindNames[] = {"none", "bool",    "u8",    "i8",    "u16",    "i16",   "u32",
                                  "i32",  "u64",     "i64",   "f32",   "f64",    "vec2",  "color",
                                  "ref",  "texture", "sound", "music", "string", "struct"};

struct Dumper {
    SaveLineSink sink = nullptr;
    void* context = nullptr;
    char line[600] = {};
    std::size_t length = 0;

    void start(int indent) {
        length = 0;
        for (int i = 0; i < indent && length + 1 < sizeof line; ++i)
            line[length++] = ' ';
        line[length] = '\0';
    }
    void add(const char* format, ...) NNH_PRINTF_FORMAT(2, 3) {
        if (length + 1 >= sizeof line) return;
        va_list args;
        va_start(args, format);
        const int n = std::vsnprintf(line + length, sizeof line - length, format, args);
        va_end(args);
        if (n > 0) {
            const std::size_t room = sizeof line - 1 - length;
            length += static_cast<std::size_t>(n) < room ? static_cast<std::size_t>(n) : room;
        }
    }
    void emit() { sink(context, line); }
};

const SaveFieldInfo* field_of(const SaveTypeInfo* type, std::uint32_t id) {
    for (int i = 0; type != nullptr && i < type->field_count; ++i) {
        if (type->fields[i].id == id) return &type->fields[i];
    }
    return nullptr;
}

bool is_retired(const SaveTypeInfo* type, std::uint32_t id) {
    for (int i = 0; type != nullptr && i < type->retired_count; ++i) {
        if (type->retired[i] == id) return true;
    }
    return false;
}

/// "name (id) ", "#id ", "#id (retired) ".
void label(Dumper& d, const SaveTypeInfo* type, const SaveFieldInfo* f, std::uint32_t id) {
    if (f != nullptr) {
        d.add("%s (%lu) ", f->name, static_cast<unsigned long>(id));
    } else {
        d.add("#%lu %s", static_cast<unsigned long>(id), is_retired(type, id) ? "(retired) " : "");
    }
}

void enum_name(Dumper& d, const EnumInfo* names, std::int64_t value) {
    for (int i = 0; names != nullptr && i < names->count; ++i) {
        if (names->values[i] != value) continue;
        const char* p = names->names; // "Normal, Fire, Ice": the i-th name
        for (int k = 0; k < i; ++k)
            p = std::strchr(p, ',') + 1;
        while (*p == ' ')
            ++p;
        int n = 0;
        while (p[n] != '\0' && p[n] != ',')
            ++n;
        d.add(" (%.*s)", n, p);
        return;
    }
}

void text(Dumper& d, const std::uint8_t* s) {
    d.add("\"");
    for (int i = 0; i < s[0]; ++i) {
        const std::uint8_t c = s[1 + i];
        if (c == '"' || c == '\\') {
            d.add("\\%c", c);
        } else if (c < 0x20 || c >= 0x7F) {
            d.add("\\x%02X", c);
        } else {
            d.add("%c", c);
        }
    }
    d.add("\"");
}

void number(Dumper& d, WireKind k, const std::uint8_t* p, const EnumInfo* names) {
    const int width = wire_width(k);
    std::uint64_t u = 0;
    std::memcpy(&u, p, static_cast<std::size_t>(width));
    const bool is_signed =
        k == WireKind::I8 || k == WireKind::I16 || k == WireKind::I32 || k == WireKind::I64;
    if (is_signed && width < 8 && ((u >> (8 * width - 1)) & 1u) != 0) u |= ~0ull << (8 * width);
    const auto s = static_cast<std::int64_t>(u);
    if (is_signed) {
        d.add("%lld", static_cast<long long>(s));
    } else {
        d.add("%llu", static_cast<unsigned long long>(u));
    }
    if (names != nullptr) enum_name(d, names, s);
}

void scalar(Dumper& d, WireKind k, const std::uint8_t* p, const EnumInfo* names) {
    switch (k) {
    case WireKind::Bool:
        if (*p <= 1) {
            d.add("%s", *p != 0 ? "true" : "false");
        } else {
            d.add("%u (not 0/1)", static_cast<unsigned>(*p));
        }
        return;
    case WireKind::F32: {
        float f;
        std::memcpy(&f, p, 4);
        d.add("%.9g [%08lX]", static_cast<double>(f), static_cast<unsigned long>(load_u32(p)));
        return;
    }
    case WireKind::F64: {
        double f;
        std::memcpy(&f, p, 8);
        d.add("%.17g [%016llX]", f, static_cast<unsigned long long>(load_u64(p)));
        return;
    }
    case WireKind::Vec2: {
        float x, y;
        std::memcpy(&x, p, 4);
        std::memcpy(&y, p + 4, 4);
        d.add("(%.9g, %.9g) [%08lX %08lX]", static_cast<double>(x), static_cast<double>(y),
              static_cast<unsigned long>(load_u32(p)), static_cast<unsigned long>(load_u32(p + 4)));
        return;
    }
    case WireKind::Color:
        d.add("#%02X%02X%02X%02X", p[0], p[1], p[2], p[3]);
        return;
    case WireKind::DataRef:
    case WireKind::TextureRef:
    case WireKind::SoundRef:
    case WireKind::MusicRef:
        d.add("0x%08lX", static_cast<unsigned long>(load_u32(p)));
        return;
    case WireKind::String:
        text(d, p);
        return;
    default:
        number(d, k, p, names);
        return;
    }
}

const EnumInfo* names_of(const SaveFieldInfo* f) {
    return f != nullptr && f->enum_names != nullptr ? f->enum_names() : nullptr;
}

void fields(Dumper& d, const SaveTypeInfo* type, const std::uint8_t* p, const std::uint8_t* end, int indent);
void value(Dumper& d, const SaveFieldInfo* f, std::uint8_t wire, const std::uint8_t* p, int indent);

void row(Dumper& d, const SaveTypeInfo* type, const SaveSequence& s, const std::uint8_t*& p, int indent) {
    const std::uint8_t* t = s.row_template;
    for (std::uint32_t k = 0; k < s.template_fields; ++k) {
        const std::uint8_t wire = *t++;
        const std::uint32_t id = take_varint(t);
        const SaveFieldInfo* f = field_of(type, id);
        d.start(indent);
        label(d, type, f, id);
        value(d, f, wire, p, indent);
        skip_value(wire, p);
    }
}

void sequence(Dumper& d, const SaveFieldInfo* f, WireKind kind, const std::uint8_t* p, int indent) {
    const SaveSequence s = read_sequence(kind, p);
    const std::uint8_t* item = s.items;
    if (kind == WireKind::Struct) {
        const SaveTypeInfo* nested = f != nullptr && f->kind == WireKind::Struct ? f->nested : nullptr;
        for (std::uint32_t i = 0; i < s.count; ++i) {
            d.start(indent + 2);
            d.add("[%lu]", static_cast<unsigned long>(i));
            d.emit();
            row(d, nested, s, item, indent + 4);
        }
        return;
    }
    const EnumInfo* names = names_of(f);
    const int width = wire_width(kind);
    const std::uint32_t per_line = kind == WireKind::String ? 1 : width == 1 ? 16 : width == 8 ? 4 : 8;
    for (std::uint32_t i = 0; i < s.count; ++i) {
        if (i % per_line == 0) {
            if (i > 0) d.emit();
            d.start(indent + 2);
            d.add("[%lu]", static_cast<unsigned long>(i));
        }
        d.add(" ");
        scalar(d, kind, item, names);
        item += kind == WireKind::String ? 1 + *item : static_cast<std::size_t>(width);
    }
    if (s.count > 0) d.emit();
}

void value(Dumper& d, const SaveFieldInfo* f, std::uint8_t wire, const std::uint8_t* p, int indent) {
    const WireKind kind = wire_kind(wire);
    const WireShape shape = wire_shape(wire);
    if (shape != WireShape::One) {
        const std::uint8_t* q = p;
        d.add("%s%s[%lu]", kKindNames[static_cast<int>(kind)], shape == WireShape::List ? " list" : "",
              static_cast<unsigned long>(take_varint(q)));
        d.emit();
        sequence(d, f, kind, p, indent);
        return;
    }
    d.add("%s", kKindNames[static_cast<int>(kind)]);
    if (kind == WireKind::Struct) {
        d.emit();
        const std::uint32_t size = take_varint(p);
        fields(d, f != nullptr && f->kind == WireKind::Struct ? f->nested : nullptr, p, p + size, indent + 2);
        return;
    }
    d.add(" = ");
    scalar(d, kind, p, names_of(f));
    d.emit();
}

void fields(Dumper& d, const SaveTypeInfo* type, const std::uint8_t* p, const std::uint8_t* end, int indent) {
    while (p < end) {
        const std::uint8_t wire = *p++;
        const std::uint32_t id = take_varint(p);
        const SaveFieldInfo* f = field_of(type, id);
        d.start(indent);
        label(d, type, f, id);
        value(d, f, wire, p, indent);
        skip_value(wire, p);
    }
}

} // namespace

Result<void> save_dump_bytes(const SaveTypeInfo* type, const void* data, std::size_t size, SaveLineSink sink,
                             void* context) {
    if (data == nullptr || sink == nullptr)
        return Error::make(ErrorCode::InvalidArgument, "no data or no sink");
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    auto header = parse_save_struct_header(bytes, size);
    if (!header) return header.error();
    const std::uint8_t* first = bytes + header.value().size;
    if (auto ok = save_validate_fields(first, static_cast<std::size_t>(bytes + size - first)); !ok) return ok;
    if (type != nullptr && std::strcmp(type->key, header.value().key) != 0)
        type = nullptr; // another type: ids only
    Dumper d;
    d.sink = sink;
    d.context = context;
    d.add("save '%s' v%lu (%lu bytes)", header.value().key,
          static_cast<unsigned long>(header.value().version), static_cast<unsigned long>(size));
    d.emit();
    fields(d, type, first, bytes + size, 0);
    return {};
}

Result<void> save_dump_value(const SaveTypeInfo& type, const void* value, void* scratch, std::size_t capacity,
                             SaveLineSink sink, void* context) {
    auto encoded = save_encode(type, value, scratch, capacity);
    if (!encoded) return encoded.error();
    return save_dump_bytes(&type, scratch, encoded.value(), sink, context);
}

Result<void> save_dump_file(const SaveTypeInfo* type, const char* path, void* buffer, std::size_t capacity,
                            SaveLineSink sink, void* context) {
    if (auto ok = fs::validate_path(path); !ok) return ok;
    auto read = save_read_one(path, buffer, capacity, nullptr);
    if (!read && read.error().code != ErrorCode::LimitReached) {
        char backup[fs::kMaxPathLength + 8];
        std::snprintf(backup, sizeof backup, "%s.bak", path);
        if (fs::validate_path(backup)) read = save_read_one(backup, buffer, capacity, nullptr);
    }
    if (!read) return read.error();
    return save_dump_bytes(type, buffer, read.value(), sink, context);
}

#else // Release: no dump code

Result<void> save_dump_bytes(const SaveTypeInfo*, const void*, std::size_t, SaveLineSink, void*) {
    return Error::make(ErrorCode::Unsupported, "dumps are Debug/Development only");
}
Result<void> save_dump_value(const SaveTypeInfo&, const void*, void*, std::size_t, SaveLineSink, void*) {
    return Error::make(ErrorCode::Unsupported, "dumps are Debug/Development only");
}
Result<void> save_dump_file(const SaveTypeInfo*, const char*, void*, std::size_t, SaveLineSink, void*) {
    return Error::make(ErrorCode::Unsupported, "dumps are Debug/Development only");
}

#endif

} // namespace nnh::detail

namespace nnh::save {

void log_lines(void*, const char* line) {
    NNH_LOG_INFO(saves, "%s", line);
    (void)line;
}

} // namespace nnh::save
