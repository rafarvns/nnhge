#pragma once
/// @file nnh/detail/data_schema.hpp
/// The tables behind NNH_DATA_TYPE / NNH_DATA_STRUCT / NNH_DATA_ENUM / NNH_DATA_FIELD (spec 014, decision
/// D29). Included by <nnh/data.hpp>; never include it on its own. Engine-internal, but in a public header
/// because the macros expand in game code.
///
/// Each field's kind, width, shape and capacity are deduced from the member's C++ type at compile time;
/// unsupported members stop the build with a static_assert, and impossible options with a call to a function
/// whose name says what is wrong (`data_error_...`). Types and enums are identified by detail::type_id<T>()
/// (no RTTI) and registered by static nodes (engine/core/src/data/schema_registry.*), so nothing is declared
/// in game headers. This file holds the records; data_kinds.hpp the kind deduction, data_tables.hpp the
/// builders and data_macros.hpp the macros.

#include <cstdint>

#include "nnh/detail/type_id.hpp"

namespace nnh::detail {

/// What one value of a field is. The numbers are part of the schema description and the fingerprint: append
/// only.
enum class FieldKind : std::uint8_t {
    Bool,
    Int,
    Float,
    String,
    Enum,
    Vec2,
    Color,
    Struct,
    DataRef,
    TextureRef,
    SoundRef,
    MusicRef,
};

/// How many values a field holds: one, exactly `count` (T[N]) or 0..count (data::List<T, N>).
enum class FieldShape : std::uint8_t { One, Array, List };

/// One field of a data type's schema. Built at compile time by make_field (NNH_DATA_FIELD); the converter
/// builds the same records from the schema description (no offsets there: `offset`, `stride` and
/// `items_offset` are only meaningful for compiled tables).
struct FieldInfo {
    const char* name = "";
    std::uint32_t offset = 0;       ///< of the member inside its struct
    std::uint32_t stride = 0;       ///< bytes per value (sizeof the element)
    std::uint32_t items_offset = 0; ///< List only: where the items start inside the List (the count is at 0)
    double min = 0; ///< effective range (numbers only): the options clamped to the member's type
    double max = 0;
    const void* target = nullptr; ///< Struct / Enum: that type's id; DataRef: the referenced type's id
    std::uint16_t count = 1;      ///< Array: N; List: capacity N; One: 1
    std::uint16_t capacity = 0;   ///< String: N bytes
    FieldKind kind = FieldKind::Bool;
    FieldShape shape = FieldShape::One;
    std::uint8_t width = 0; ///< bytes of a Bool, Int, Float or Enum value; 0 for other kinds
    bool is_signed = false; ///< Int / Enum
    bool required = false;  ///< a data file must set it
};

/// A data type (NNH_DATA_TYPE, with a key) or a nested-only struct (NNH_DATA_STRUCT, key "").
struct TypeInfo {
    const void* tag = nullptr; ///< type_id<T>()
    const char* name = "";     ///< the C++ name as written in the macro
    const char* key = "";      ///< file extension of its data assets; "" for NNH_DATA_STRUCT
    const char* source = "";   ///< the schema source that declares it (for messages)
    const FieldInfo* fields = nullptr;
    std::uint16_t field_count = 0;
    std::uint32_t size = 0;         ///< sizeof(T); 0 in tables read from a description
    void (*reset)(void*) = nullptr; ///< constructs T{} in place (T is trivially destructible)
};

/// A game enum written by name in data files (NNH_DATA_ENUM).
struct EnumInfo {
    const void* tag = nullptr;
    const char* name = "";
    const char* names = "";               ///< the names as one string: "Normal, Fire, Ice"
    const std::int64_t* values = nullptr; ///< value of each name, in order
    const char* source = "";              ///< the schema source that declares it (for messages)
    std::uint16_t count = 0;
    std::uint8_t width = 0;
    bool is_signed = false;
};

/// Registration nodes: one per NNH_DATA_TYPE/STRUCT/ENUM, linked into the program's registry before main().
struct TypeNode {
    const TypeInfo* info = nullptr;
    TypeNode* next = nullptr;
};
struct EnumNode {
    const EnumInfo* info = nullptr;
    EnumNode* next = nullptr;
};
struct TypeRegistrar {
    explicit TypeRegistrar(TypeNode& node);
};
struct EnumRegistrar {
    explicit EnumRegistrar(EnumNode& node);
};

// Mistakes found while building the tables. Never defined: calling one in a constant expression stops the
// build, and the compiler's message names the function.
void data_error_min_or_max_on_a_field_that_is_not_a_number();
void data_error_min_or_max_of_an_integer_field_must_be_a_whole_number();
void data_error_min_is_greater_than_max();
void data_error_field_listed_twice();
void data_error_enum_names_and_values_differ();

} // namespace nnh::detail

#include "nnh/detail/data_kinds.hpp"
#include "nnh/detail/data_tables.hpp"
#include "nnh/detail/data_macros.hpp"
