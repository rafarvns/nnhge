// Applying a validated structured save to a struct (save_apply.hpp).

#include "saves/save_apply.hpp"

#include "saves/save_convert.hpp"
#include "saves/save_cursor.hpp"

#include <cstring>

namespace nnh::detail {
namespace {

using save::WireKind;
using save::WireShape;

constexpr int kMaxCount = 65535;

/// Whether `id` is one of the type's retired ids (`next` walks the sorted list once per struct).
bool retired(const SaveTypeInfo& type, int& next, std::uint32_t id) {
    while (next < type.retired_count && type.retired[next] < id)
        ++next;
    return next < type.retired_count && type.retired[next] == id;
}

bool is_integer(WireKind k) {
    return k >= WireKind::U8 && k <= WireKind::I64;
}

/// Whether a value of `from` can ever become a value of `to` (String and Struct only convert to themselves).
bool compatible(WireKind from, WireKind to) {
    const bool special_from = from == WireKind::String || from == WireKind::Struct;
    const bool special_to = to == WireKind::String || to == WireKind::Struct;
    return special_from || special_to ? from == to : true;
}

bool apply_one(SaveApply& a, const SaveFieldInfo& f, std::uint8_t* dst, WireKind from,
               const std::uint8_t* src, const EnumInfo* names) {
    if (!compatible(from, f.kind)) return false;
    if (from == WireKind::String) {
        if (convert_string(src, f.capacity, dst)) a.note(SaveNote::Adjusted);
        return true;
    }
    if (from == WireKind::Struct) {
        const std::uint8_t* p = src;
        const std::uint32_t size = take_varint(p);
        apply_fields(a, *f.nested, dst, p, p + size);
        return true;
    }
    return convert_scalar(from, src, f, names, dst);
}

/// A list's items start from their defaults: [0, max(old count, n)) reset, then the count set to n.
std::uint8_t* open_list(const SaveFieldInfo& f, std::uint8_t* member, std::uint32_t n) {
    std::uint8_t* items = member + f.items_offset;
    std::uint32_t old = load_u16(member);
    if (old > f.count) old = f.count;
    const std::uint32_t used = old > n ? old : n;
    if (f.kind == WireKind::Struct) {
        for (std::uint32_t i = 0; i < used; ++i)
            f.nested->reset(items + static_cast<std::size_t>(i) * f.stride);
    } else {
        std::memset(items, 0, static_cast<std::size_t>(used) * f.stride);
    }
    store_u16(member, static_cast<std::uint16_t>(n));
    return items;
}

bool apply_sequence(SaveApply& a, const SaveFieldInfo& f, std::uint8_t* member, WireKind from,
                    const std::uint8_t* value, const EnumInfo* names) {
    const std::uint8_t* p = value;
    const SaveSequence s = read_sequence(from, p);
    if (!compatible(from, f.kind)) return false;
    std::uint32_t n = s.count;
    if (n > f.count) {
        n = f.count;
        a.note(SaveNote::Adjusted);
    }
    std::uint8_t* items = f.shape == WireShape::List ? open_list(f, member, n) : member;
    a.visited += n;
    if (from == WireKind::Struct) {
        const std::uint8_t* row = s.items;
        for (std::uint32_t i = 0; i < n; ++i) {
            a.enter(static_cast<std::uint16_t>(i), true);
            apply_row(a, *f.nested, items + static_cast<std::size_t>(i) * f.stride, s.row_template,
                      s.template_fields, row);
            a.leave();
        }
        return true;
    }
    if (from == WireKind::String) {
        const std::uint8_t* q = s.items;
        for (std::uint32_t i = 0; i < n; ++i) {
            if (convert_string(q, f.capacity, items + static_cast<std::size_t>(i) * f.stride)) {
                a.enter(static_cast<std::uint16_t>(i), true);
                a.note(SaveNote::Adjusted);
                a.leave();
            }
            q += 1 + *q;
        }
        return true;
    }
    if (copies_as_is(from, f) || (from == f.kind && names == nullptr && is_integer(from))) {
        std::memcpy(items, s.items, static_cast<std::size_t>(n) * f.stride); // the packed copy
        return true;
    }
    if (from == f.kind &&
        (from == WireKind::Bool || from == WireKind::F32)) { // the same kind: a check per item
        for (std::uint32_t i = 0; i < n; ++i) {
            const bool ok = from == WireKind::Bool ? s.items[i] <= 1 : f32_finite(load_u32(s.items + 4 * i));
            if (!ok) {
                a.enter(static_cast<std::uint16_t>(i), true);
                a.note(SaveNote::Dropped);
                a.leave();
            } else if (from == WireKind::Bool) {
                items[i] = s.items[i];
            } else {
                std::memcpy(items + static_cast<std::size_t>(i) * 4, s.items + 4 * i, 4);
            }
        }
        return true;
    }
    const int width = wire_width(from);
    for (std::uint32_t i = 0; i < n; ++i) {
        if (!convert_scalar(from, s.items + static_cast<std::size_t>(i) * static_cast<std::size_t>(width), f,
                            names, items + static_cast<std::size_t>(i) * f.stride)) {
            a.enter(static_cast<std::uint16_t>(i), true);
            a.note(SaveNote::Dropped);
            a.leave();
        }
    }
    return true;
}

} // namespace

void SaveApply::note(SaveNote what, std::uint16_t id) {
    int* count = what == SaveNote::Unknown   ? &report->unknown
                 : what == SaveNote::Dropped ? &report->dropped
                                             : &report->adjusted;
    if (*count < kMaxCount) ++*count;
    if constexpr (kSaveKeepPaths) {
        save::FieldPath* at = what == SaveNote::Unknown   ? report->unknown_at
                              : what == SaveNote::Dropped ? report->dropped_at
                                                          : report->adjusted_at;
        if (*count <= save::kReportPaths) {
            at[*count - 1] = path;
            if (id != 0) at[*count - 1].push(id, false);
        }
    }
}

bool apply_field(SaveApply& a, const SaveFieldInfo& f, std::uint8_t* member, std::uint8_t wire,
                 const std::uint8_t* value) {
    const WireKind from = wire_kind(wire);
    const bool one = wire_shape(wire) == WireShape::One;
    const EnumInfo* names = a.enum_names(f);
    const bool applied = one != (f.shape == WireShape::One) ? false
                         : one ? apply_one(a, f, member, from, value, names)
                               : apply_sequence(a, f, member, from, value, names);
    if (!applied) a.note(SaveNote::Dropped);
    return applied;
}

namespace {

/// Where a merge of file ids with a struct's sorted fields and retired ids has got to.
struct Merge {
    int field = 0;
    int retired = 0;
};

/// One file value of id `id`: applied to the field of that id, skipped silently when retired, else counted
/// unknown.
void apply_by_id(SaveApply& a, const SaveTypeInfo& type, Merge& m, std::uint8_t* base, std::uint8_t wire,
                 std::uint32_t id, const std::uint8_t* value) {
    ++a.visited;
    while (m.field < type.field_count && type.fields[m.field].id < id)
        ++m.field;
    if (m.field < type.field_count && type.fields[m.field].id == id) {
        const SaveFieldInfo& f = type.fields[m.field];
        a.enter(static_cast<std::uint16_t>(id), false);
        apply_field(a, f, base + f.offset, wire, value);
        a.leave();
    } else if (!retired(type, m.retired, id)) {
        a.note(SaveNote::Unknown, static_cast<std::uint16_t>(id));
    }
}

} // namespace

void apply_fields(SaveApply& a, const SaveTypeInfo& type, std::uint8_t* base, const std::uint8_t* p,
                  const std::uint8_t* end) {
    Merge merge;
    while (p < end) {
        const std::uint8_t wire = *p++;
        const std::uint32_t id = take_varint(p);
        const std::uint8_t* value = p;
        skip_value(wire, p);
        apply_by_id(a, type, merge, base, wire, id, value);
    }
}

void apply_row(SaveApply& a, const SaveTypeInfo& type, std::uint8_t* item, const std::uint8_t* row_template,
               std::uint32_t template_fields, const std::uint8_t*& row) {
    const std::uint8_t* t = row_template;
    Merge merge;
    for (std::uint32_t k = 0; k < template_fields; ++k) {
        const std::uint8_t wire = *t++;
        const std::uint32_t id = take_varint(t);
        const std::uint8_t* value = row;
        skip_value(wire, row);
        apply_by_id(a, type, merge, item, wire, id, value);
    }
}

} // namespace nnh::detail
