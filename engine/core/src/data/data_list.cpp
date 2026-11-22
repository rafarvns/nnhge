// Listing data assets and turning references into paths (spec 014 FR-8, FR-12, FR-13): count, name, ref,
// find, path, plus preload, release and memory_used. Each loads the index on first use; none allocates after
// that.

#include <cstring>

#include "data/data_index.hpp"
#include "nnh/data.hpp"
#include "nnh/detail/name_hash.hpp"

namespace nnh {

namespace detail {

namespace {

static_assert(static_cast<int>(DataRefKind::Texture) == static_cast<int>(idxfmt::Group::Texture) &&
              static_cast<int>(DataRefKind::Music) == static_cast<int>(idxfmt::Group::Music));

/// T's position in the index (its first entry and count), or false (logged once per call) when unknown.
bool span_of(TypeId type, IndexType& out) {
    if (!data_index_load()) return false;
    const DataTypeEntry* t = data_type(type);
    if (t == nullptr || t->index_type < 0) {
        NNH_LOG_WARN(data, "data: listing a type that has no data assets in this build");
        return false;
    }
    out = data_index().type(t->index_type);
    return true;
}

} // namespace

int data_count(TypeId type) {
    IndexType t;
    return span_of(type, t) ? t.count : 0;
}

const char* data_name(TypeId type, int index) {
    IndexType t;
    if (!span_of(type, t) || index < 0 || index >= t.count) return "";
    return data_index().path(t.first + index);
}

std::uint32_t data_ref(TypeId type, int index) {
    IndexType t;
    if (!span_of(type, t) || index < 0 || index >= t.count) return 0;
    return data_index().hash(t.first + index);
}

std::uint32_t data_find(TypeId type, const char* path) {
    IndexType t;
    if (path == nullptr || !span_of(type, t)) return 0;
    const std::uint32_t hash = fnv1a32(path);
    const int entry = data_index().find(idxfmt::Group::Data, hash);
    if (entry < t.first || entry >= t.first + t.count || std::strcmp(data_index().path(entry), path) != 0)
        return 0;
    return hash;
}

const char* data_path(DataRefKind kind, std::uint32_t id) {
    if (id == 0 || !data_index_load()) return "";
    const int entry = data_index().find(static_cast<idxfmt::Group>(kind), id);
    return entry < 0 ? "" : data_index().path(entry);
}

} // namespace detail

Result<void> data::preload() {
    return detail::data_index_load();
}

void data::release() {
    detail::data_index_release();
}

std::size_t data::memory_used() {
    return mem::tag_stats(detail::data_memory_tag()).bytes;
}

} // namespace nnh
