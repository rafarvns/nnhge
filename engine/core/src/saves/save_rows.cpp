// Applying struct rows by a plan of one action per column (save_rows.hpp).

#include "saves/save_rows.hpp"

#include "saves/save_bulk.hpp"
#include "saves/save_convert.hpp"

namespace nnh::detail {
namespace {

using save::WireKind;
using save::WireShape;

/// What a column's value does to its row's item.
enum class Action : std::uint8_t {
    Skip,    ///< a retired id: nothing, not counted
    Unknown, ///< an id the struct doesn't declare: counted unknown
    Copy1,   ///< the same kind, copied as it is (integers, colours, references): 1, 2, 4 or 8 bytes
    Copy2,
    Copy4,
    Copy8,
    Bool,    ///< the same kind, checked: a byte above 1 is dropped
    F32,     ///< the same kind, checked: a float that isn't finite is dropped
    String,  ///< a string into a string field: cut at a character boundary when longer (adjusted)
    Convert, ///< anything else: apply_field (conversions, structs, sequences, another kind or shape)
};

struct Column { // 6 bytes
    std::uint16_t id;
    std::uint16_t offset; ///< the field's offset in the item (a save struct is under 64 KiB)
    std::uint8_t wire;
    Action action;
    std::uint8_t aux; ///< String: the field's capacity; Convert: the field's index in the item struct
};

Action action_of(std::uint8_t wire, const SaveFieldInfo& f) {
    const WireKind kind = wire_kind(wire);
    if (wire_shape(wire) != WireShape::One || f.shape != WireShape::One) return Action::Convert;
    if (copies_as_is(kind, f)) {
        const int width = wire_width(kind);
        return width == 1   ? Action::Copy1
               : width == 2 ? Action::Copy2
               : width == 4 ? Action::Copy4
                            : Action::Copy8;
    }
    if (kind != f.kind) return Action::Convert;
    if (kind == WireKind::Bool) return Action::Bool;
    if (kind == WireKind::F32) return Action::F32;
    return kind == WireKind::String ? Action::String : Action::Convert;
}

/// Matches the template's ids with `type`'s sorted fields and retired ids (apply_row's merge, done once).
/// False when the template has more columns than a plan holds.
bool plan_columns(const SaveTypeInfo& type, const SaveSequence& s, Column* plan) {
    if (s.template_fields > kRowPlanColumns) return false;
    const std::uint8_t* t = s.row_template;
    int field = 0;
    int retired = 0;
    for (std::uint32_t k = 0; k < s.template_fields; ++k) {
        Column& c = plan[k];
        c.wire = *t++;
        c.id = static_cast<std::uint16_t>(take_varint(t));
        while (field < type.field_count && type.fields[field].id < c.id)
            ++field;
        c.offset = 0;
        c.aux = 0;
        if (field < type.field_count && type.fields[field].id == c.id) {
            const SaveFieldInfo& f = type.fields[field];
            c.action = action_of(c.wire, f);
            c.offset = static_cast<std::uint16_t>(f.offset);
            c.aux = c.action == Action::String ? f.capacity : static_cast<std::uint8_t>(field);
        } else {
            c.action = save_id_retired(type, retired, c.id) ? Action::Skip : Action::Unknown;
        }
    }
    return true;
}

/// A value of the current row didn't fit field `id`: counted at the row's path followed by `id`.
void note_at(SaveApply& a, std::uint16_t id, SaveNote what) {
    a.enter(id, false);
    a.note(what);
    a.leave();
}

/// Moves `p` past a value of `wire` (skip_value on a copy: `p` itself stays in a register).
const std::uint8_t* skipped(std::uint8_t wire, const std::uint8_t* p) {
    skip_value(wire, p);
    return p;
}

/// Applies one row by `plan` to the item at `item`, starting at `p`. Returns the row's end.
const std::uint8_t* apply_planned(SaveApply& a, const SaveTypeInfo& type, const Column* plan,
                                  std::uint32_t columns, std::uint8_t* item, const std::uint8_t* p) {
    for (std::uint32_t k = 0; k < columns; ++k) {
        const Column& c = plan[k];
        std::uint8_t* dst = item + c.offset;
        switch (c.action) {
        case Action::Copy1:
            *dst = *p++;
            break;
        case Action::Copy2:
            copy_value(dst, p, 2);
            p += 2;
            break;
        case Action::Copy4:
            copy_value(dst, p, 4);
            p += 4;
            break;
        case Action::Copy8:
            copy_value(dst, p, 8);
            p += 8;
            break;
        case Action::Bool:
            if (*p <= 1) {
                *dst = *p;
            } else {
                note_at(a, c.id, SaveNote::Dropped);
            }
            ++p;
            break;
        case Action::F32:
            if (f32_finite(load_u32(p))) {
                copy_value(dst, p, 4);
            } else {
                note_at(a, c.id, SaveNote::Dropped);
            }
            p += 4;
            break;
        case Action::String:
            if (convert_string(p, c.aux, dst)) note_at(a, c.id, SaveNote::Adjusted);
            p += 1 + *p;
            break;
        case Action::Convert: {
            const std::uint8_t* value = p;
            p = skipped(c.wire, p);
            a.enter(c.id, false);
            apply_field(a, type.fields[c.aux], dst, c.wire, value);
            a.leave();
            break;
        }
        case Action::Unknown:
            a.note(SaveNote::Unknown, c.id);
            p = skipped(c.wire, p);
            break;
        case Action::Skip:
            p = skipped(c.wire, p);
            break;
        }
    }
    return p;
}

} // namespace

void apply_rows(SaveApply& a, const SaveFieldInfo& f, std::uint8_t* items, const SaveSequence& s,
                std::uint32_t n) {
    const SaveTypeInfo& type = *f.nested;
    Column plan[kRowPlanColumns];
    const bool planned = plan_columns(type, s, plan);
    const std::uint8_t* row = s.items;
    for (std::uint32_t i = 0; i < n; ++i) {
        std::uint8_t* item = items + static_cast<std::size_t>(i) * f.stride;
        a.enter(static_cast<std::uint16_t>(i), true);
        if (planned) {
            a.visited += s.template_fields; // apply_row counts each value
            row = apply_planned(a, type, plan, s.template_fields, item, row);
        } else {
            apply_row(a, type, item, s.row_template, s.template_fields, row);
        }
        a.leave();
    }
}

} // namespace nnh::detail
