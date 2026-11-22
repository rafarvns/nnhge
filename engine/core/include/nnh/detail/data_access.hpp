#pragma once
/// @file nnh/detail/data_access.hpp
/// The non-template functions behind nnh::data::load / count / name / ref / find / path (spec 014): the
/// templates in <nnh/data.hpp> pass the type's id (detail::type_id<T>()), so no game code is generated per
/// call beyond a forwarding line. Defined in engine/core/src/data/data_load.cpp and data_list.cpp. Included
/// by <nnh/data.hpp>; never include it on its own.

#include <cstdint>

#include "nnh/detail/type_id.hpp"
#include "nnh/result.hpp"

namespace nnh::detail {

/// What a reference names (the index group it is looked up in).
enum class DataRefKind : std::uint8_t { Data, Texture, Sound, Music };

Result<void> data_load(TypeId type, const char* path, void* out);
Result<void> data_load_ref(TypeId type, std::uint32_t id, void* out);
int data_count(TypeId type);
const char* data_name(TypeId type, int index);
std::uint32_t data_ref(TypeId type, int index);
std::uint32_t data_find(TypeId type, const char* path);
const char* data_path(DataRefKind kind, std::uint32_t id);

} // namespace nnh::detail
