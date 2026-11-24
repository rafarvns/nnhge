#pragma once
// Walking a structured save that save_validate_fields() accepted (spec 027): skipping a value by its lengths
// and reading a sequence's header. No bounds checks here: only validated bytes reach these functions. Used by
// the decoder (save_apply.cpp), the migration view (old_fields.cpp) and the dump (save_dump.cpp).

#include "saves/save_wire.hpp"

#include <cstdint>

namespace nnh::detail {

/// A sequence's header: its items (fixed kinds: packed; strings: one after another; structs: rows).
struct SaveSequence {
    std::uint32_t count = 0;
    const std::uint8_t* row_template = nullptr; ///< structs: the template's first wire byte
    std::uint32_t template_fields = 0;          ///< structs: fields in a row
    const std::uint8_t* items = nullptr;
    const std::uint8_t* end = nullptr; ///< just past the last item
};

/// Reads the header of a sequence of `kind` at `p` (just past its tag) and moves `p` past the whole sequence.
inline SaveSequence read_sequence(save::WireKind kind, const std::uint8_t*& p) {
    SaveSequence s;
    s.count = take_varint(p);
    if (kind == save::WireKind::Struct) {
        s.template_fields = take_varint(p);
        s.row_template = p;
        for (std::uint32_t i = 0; i < s.template_fields; ++i) {
            ++p;
            take_varint(p);
        }
    }
    if (kind == save::WireKind::Struct || kind == save::WireKind::String) {
        const std::uint32_t bytes = take_varint(p);
        s.items = p;
        p += bytes;
    } else {
        s.items = p;
        p += static_cast<std::size_t>(s.count) * static_cast<std::size_t>(wire_width(kind));
    }
    s.end = p;
    return s;
}

/// Moves `p` past one value of `wire` (One: its bytes, string or struct; a sequence: all of it).
inline void skip_value(std::uint8_t wire, const std::uint8_t*& p) {
    const save::WireKind kind = wire_kind(wire);
    if (wire_shape(wire) != save::WireShape::One) {
        read_sequence(kind, p);
    } else if (kind == save::WireKind::String) {
        p += 1 + *p;
    } else if (kind == save::WireKind::Struct) {
        const std::uint32_t size = take_varint(p);
        p += size;
    } else {
        p += wire_width(kind);
    }
}

} // namespace nnh::detail
