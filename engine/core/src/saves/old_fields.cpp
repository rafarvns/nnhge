// The migration steps' view of a file's fields (spec 027, D46, FR-13): lookups by a path of ids and item
// indexes over the validated bytes (a level's field headers are walked, lengths make each skip O(1)), and
// conversions by the load's own rules (save_apply).

#include "saves/save_apply.hpp"
#include "saves/save_cursor.hpp"

namespace nnh::detail {
namespace {

using save::WireKind;
using save::WireShape;

/// What a path names: a value (tagged, or an item of a sequence), or a row of a struct sequence.
struct Spot {
    enum class What : std::uint8_t { None, Value, Row };
    What what = What::None;
    std::uint8_t wire = 0;                      ///< Value: its wire byte (an item's shape is One)
    const std::uint8_t* at = nullptr;           ///< Value: its bytes; Row: the row's first value
    const std::uint8_t* row_template = nullptr; ///< Row
    std::uint32_t template_fields = 0;          ///< Row
};

Spot value_spot(std::uint8_t wire, const std::uint8_t* at) {
    Spot s;
    s.what = Spot::What::Value;
    s.wire = wire;
    s.at = at;
    return s;
}

/// Field `id` among the tagged fields [p, end) (ids ascend: the walk stops past it).
Spot find_tagged(const std::uint8_t* p, const std::uint8_t* end, std::uint32_t id) {
    while (p < end) {
        const std::uint8_t wire = *p++;
        const std::uint32_t found = take_varint(p);
        if (found == id) return value_spot(wire, p);
        if (found > id) break;
        skip_value(wire, p);
    }
    return {};
}

/// Field `id` of a row: the template and the row's values walked together.
Spot find_in_row(const Spot& row, std::uint32_t id) {
    const std::uint8_t* t = row.row_template;
    const std::uint8_t* p = row.at;
    for (std::uint32_t k = 0; k < row.template_fields; ++k) {
        const std::uint8_t wire = *t++;
        if (take_varint(t) == id) return value_spot(wire, p);
        skip_value(wire, p);
    }
    return {};
}

/// Item `index` of the sequence `s` names.
Spot find_item(const Spot& s, std::uint32_t index) {
    if (s.what != Spot::What::Value || wire_shape(s.wire) == WireShape::One) return {};
    const WireKind kind = wire_kind(s.wire);
    const std::uint8_t* p = s.at;
    const SaveSequence seq = read_sequence(kind, p);
    if (index >= seq.count) return {};
    const std::uint8_t* item = seq.items;
    if (kind == WireKind::Struct) {
        for (std::uint32_t r = 0; r < index; ++r) {
            const std::uint8_t* t = seq.row_template;
            for (std::uint32_t k = 0; k < seq.template_fields; ++k) {
                const std::uint8_t wire = *t++;
                take_varint(t);
                skip_value(wire, item);
            }
        }
        Spot row;
        row.what = Spot::What::Row;
        row.at = item;
        row.row_template = seq.row_template;
        row.template_fields = seq.template_fields;
        return row;
    }
    if (kind == WireKind::String) {
        for (std::uint32_t r = 0; r < index; ++r)
            item += 1 + *item;
    } else {
        item += static_cast<std::size_t>(index) * static_cast<std::size_t>(wire_width(kind));
    }
    return value_spot(wire_byte(kind, WireShape::One), item);
}

Spot locate(const std::uint8_t* root, std::uint32_t size, const save::FieldPath& path) {
    if (root == nullptr || !path.valid() || path.size() == 0 || path.is_item(0)) return {};
    Spot s = find_tagged(root, root + size, path.at(0));
    for (int i = 1; i < path.size() && s.what != Spot::What::None; ++i) {
        if (path.is_item(i)) {
            s = find_item(s, path.at(i));
        } else if (s.what == Spot::What::Row) {
            s = find_in_row(s, path.at(i));
        } else if (wire_kind(s.wire) == WireKind::Struct && wire_shape(s.wire) == WireShape::One) {
            const std::uint8_t* p = s.at;
            const std::uint32_t length = take_varint(p);
            s = find_tagged(p, p + length, path.at(i));
        } else {
            s = {};
        }
    }
    return s;
}

} // namespace

bool old_fields_get(const std::uint8_t* root, std::uint32_t size, const save::FieldPath& path,
                    const SaveFieldInfo& field, void* out) {
    const Spot s = locate(root, size, path);
    if (s.what == Spot::What::None) return false;
    save::Report scratch;
    SaveApply apply;
    apply.report = &scratch;
    auto* target = static_cast<std::uint8_t*>(out);
    if (s.what == Spot::What::Row) {
        if (field.kind != WireKind::Struct || field.shape != WireShape::One) return false;
        const std::uint8_t* row = s.at;
        apply_row(apply, *field.nested, target, s.row_template, s.template_fields, row);
        return true;
    }
    return apply_field(apply, field, target, s.wire, s.at);
}

} // namespace nnh::detail

namespace nnh::save {

bool OldFields::has(const FieldPath& path) const {
    return detail::locate(root_, size_, path).what != detail::Spot::What::None;
}

WireKind OldFields::kind(const FieldPath& path) const {
    const detail::Spot s = detail::locate(root_, size_, path);
    if (s.what == detail::Spot::What::Row) return WireKind::Struct;
    return s.what == detail::Spot::What::Value ? detail::wire_kind(s.wire) : WireKind::None;
}

WireShape OldFields::shape(const FieldPath& path) const {
    const detail::Spot s = detail::locate(root_, size_, path);
    return s.what == detail::Spot::What::Value ? detail::wire_shape(s.wire) : WireShape::One;
}

int OldFields::count(const FieldPath& path) const {
    const detail::Spot s = detail::locate(root_, size_, path);
    if (s.what != detail::Spot::What::Value || detail::wire_shape(s.wire) == WireShape::One) return -1;
    const std::uint8_t* p = s.at;
    return static_cast<int>(detail::take_varint(p));
}

} // namespace nnh::save
