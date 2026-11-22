#pragma once
// Names of a data enum (spec 014 FR-2): NNH_DATA_ENUM keeps them as one stringified list, "Normal, Fire,
// Ice"; the description reader builds the same kind of list. Pure; shared with the schema host program and
// nnh-assetc. Tests: tests/data_assets/test_enum_names.cpp.

namespace nnh::detail {

/// A piece of a longer string (not NUL-terminated).
struct NameView {
    const char* text = "";
    int length = 0;
};

/// Name `index` of the comma-separated list `names`, spaces trimmed; empty when out of range.
NameView enum_name(const char* names, int index);

/// Index of the name `name[0..length)` in `names` (whole names, case-sensitive); -1 when absent or empty.
int enum_index(const char* names, const char* name, int length);

} // namespace nnh::detail
