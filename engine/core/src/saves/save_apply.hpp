#pragma once
// Applying a validated structured save to a struct (spec 027, D46): fields merged by id with the schema's
// sorted fields (nothing quadratic), sequences item by item or as one copy, rows in lockstep with their
// template, every conversion through save_convert, and the report's counts (paths in Debug/Development).
// Tested in tests/saves/test_round_trip.cpp, test_evolution.cpp, test_fit.cpp.

#include "nnh/saves.hpp"

#include <cstdint>

namespace nnh::detail {

/// Paths are kept in Debug and Development builds only.
#if defined(NNH_FLAVOR_RELEASE)
inline constexpr bool kSaveKeepPaths = false;
#else
inline constexpr bool kSaveKeepPaths = true;
#endif

enum class SaveNote : std::uint8_t { Unknown, Dropped, Adjusted };

struct SaveApply {
    save::Report* report = nullptr;
    save::FieldPath path;      ///< where the decoder is (Debug/Development)
    std::uint32_t visited = 0; ///< values applied or skipped (the linear-cost test)

    void enter(std::uint16_t step, bool item) {
        if constexpr (kSaveKeepPaths) path.push(step, item);
    }
    void leave() {
        if constexpr (kSaveKeepPaths) path.pop();
    }
    /// Counts one unknown, dropped or adjusted value at the current path followed by `id` (0: the path
    /// itself).
    void note(SaveNote what, std::uint16_t id = 0);

    /// The NNH_DATA_ENUM names of an enum field (nullptr for other fields and enums without names). The last
    /// lookup is kept, so the rows of a list search the registry once.
    const EnumInfo* enum_names(const SaveFieldInfo& f) {
        if (f.enum_names == nullptr) return nullptr;
        if (f.enum_names != last_lookup_) {
            last_lookup_ = f.enum_names;
            last_names_ = f.enum_names();
        }
        return last_names_;
    }

  private:
    const EnumInfo* (*last_lookup_)() = nullptr;
    const EnumInfo* last_names_ = nullptr;
};

/// Applies the tagged fields [p, end) to the struct of `type` at `base` (already T{} or the caller's
/// defaults).
void apply_fields(SaveApply& a, const SaveTypeInfo& type, std::uint8_t* base, const std::uint8_t* p,
                  const std::uint8_t* end);

/// Applies one value of `wire` at `value` to field `f` at `member`. Returns false when nothing of it fit
/// (another kind or shape, or a single value that didn't fit; counted as dropped).
bool apply_field(SaveApply& a, const SaveFieldInfo& f, std::uint8_t* member, std::uint8_t wire,
                 const std::uint8_t* value);

/// Applies one row (values in template order) to the struct of `type` at `item`; moves `row` past it.
void apply_row(SaveApply& a, const SaveTypeInfo& type, std::uint8_t* item, const std::uint8_t* row_template,
               std::uint32_t template_fields, const std::uint8_t*& row);

} // namespace nnh::detail
