#pragma once
/// @file nnh/detail/save_schema.hpp
/// The records behind NNH_SAVE_TYPE / NNH_SAVE_STRUCT / NNH_SAVE_SUMMARY (spec 027, decision D45), and the
/// non-template functions the templates of <nnh/saves.hpp> call. Included by <nnh/saves.hpp>; never include
/// it on its own. Engine-internal, but in a public header because the macros expand in game code.
///
/// A declaration expands to a struct of constexpr tables beside the game's struct and a constexpr function
/// `nnh_save_info(SaveTag<T>)` returning its record, found by argument-dependent lookup: no registry, no
/// static initialiser, nested structs found at compile time. Mistakes stop the build through a static_assert
/// or a call to an undefined `save_error_...` function in the constant expression (the compiler's message
/// names it).

#include <cstddef>
#include <cstdint>

namespace nnh::save {
class OldFields;
struct Report;
struct FileInfo;
} // namespace nnh::save

namespace nnh::detail {

struct SaveTypeInfo;
struct EnumInfo; // <nnh/detail/data_schema.hpp>

/// One saved field: a member of a struct, or the value OldFields::get converts into.
struct SaveFieldInfo {
    const char* name = "";                ///< the member's name ("" in Release)
    const SaveTypeInfo* nested = nullptr; ///< Struct: the struct's record
    const EnumInfo* (*enum_names)() =
        nullptr;                    ///< an enum: its NNH_DATA_ENUM names (nullptr when it has none)
    std::uint32_t offset = 0;       ///< of the member inside its struct
    std::uint32_t stride = 0;       ///< bytes per value (sizeof the element)
    std::uint32_t items_offset = 0; ///< List: where the items start (the 16-bit count is at 0)
    std::uint16_t count = 1;        ///< Array: N; List: its capacity; One: 1
    std::uint16_t id = 0;           ///< 1..65535
    std::uint8_t capacity = 0;      ///< String: N bytes
    save::WireKind kind = save::WireKind::None;
    save::WireShape shape = save::WireShape::One;
    std::uint8_t width = 0; ///< bytes of the member's value for fixed kinds (an enum: its size)
};

/// A migration step: converts a value of version `from` into version from + 1.
struct SaveStep {
    std::uint32_t from = 0;
    void (*run)(void* value, const save::OldFields& old) = nullptr;
};

enum class SaveRole : std::uint8_t {
    Struct,  ///< NNH_SAVE_STRUCT: nested only, no key or version
    Type,    ///< NNH_SAVE_TYPE: a file's root, stored and loaded
    Summary, ///< NNH_SAVE_SUMMARY: loads a subset of a type's ids; never stored
};

/// A save type, struct or summary. Fields sorted by id, retired ids sorted, steps sorted by `from`.
struct SaveTypeInfo {
    const char* name = ""; ///< the C++ name as written in the macro ("" in Release)
    const char* key = "";  ///< "" for a struct
    const SaveFieldInfo* fields = nullptr;
    const std::uint16_t* retired = nullptr;
    const SaveStep* steps = nullptr;
    void (*reset)(void*) = nullptr; ///< constructs T{} in place (T is trivially destructible)
    std::uint32_t size = 0;         ///< sizeof(T)
    std::uint32_t max_fields =
        0;                     ///< largest encoding of its fields, tags included (a struct value's content)
    std::uint32_t max_row = 0; ///< largest row in a sequence: every value, no tags
    std::uint32_t template_size =
        0;                     ///< its template in a sequence: count varint + wire byte and id per field
    std::uint32_t version = 0; ///< current version (type), highest read (summary); 0 for a struct
    std::uint32_t key_hash = 0;
    std::uint16_t field_count = 0;
    std::uint16_t retired_count = 0;
    std::uint16_t step_count = 0;
    std::uint8_t depth = 1; ///< struct levels, itself included
    SaveRole role = SaveRole::Struct;
};

/// The ADL key of a save declaration: `nnh_save_info(SaveTag<T>{})` finds T's record in T's namespace.
template <class T> struct SaveTag {};

/// Never defined: only stops an unqualified call from finding nothing at the template's definition.
void nnh_save_info() = delete;

// Mistakes found while building the tables. Never defined: calling one in a constant expression stops the
// build, and the compiler's message names the function.
void save_error_field_id_must_be_1_to_65535();
void save_error_field_id_used_twice();
void save_error_retired_id_must_be_1_to_65535();
void save_error_retired_id_listed_twice();
void save_error_retired_id_used_by_a_field();
void save_error_more_than_64_retired_ids();
void save_error_more_than_128_fields();
void save_error_a_save_struct_needs_a_field();
void save_error_nesting_deeper_than_8_levels();
void save_error_max_encoded_size_over_256_KiB();
void save_error_bad_key_use_1_to_16_of_a_to_z_0_to_9_underscore_starting_with_a_letter();
void save_error_version_must_be_at_least_1();
void save_error_version_given_twice();
void save_error_step_from_a_version_not_below_the_current_one();
void save_error_two_steps_for_one_version();
void save_error_missing_migration_step_every_version_below_the_current_one_needs_one();
void save_error_NNH_SAVE_VERSION_or_NNH_SAVE_STEP_in_a_struct_or_summary();

// ---- The non-template functions behind <nnh/saves.hpp>'s templates (engine/core/src/saves/)
// ----------------

std::size_t save_max_size(const SaveTypeInfo& type);
const EnumInfo*
save_find_enum_names(const void* tag); ///< save_enum.cpp: links the data registry only when used

} // namespace nnh::detail
