// nnh::data::load by path and by reference (spec 014 FR-8, FR-11): index entry → read into the scratch buffer
// → validate against the game's compiled type (fingerprint included) → T{} in place → apply. Every failure
// returns before `out` is touched.

#include <cstdio>
#include <cstring>

#include "assets/asset_path.hpp"
#include "core/engine_state.hpp"
#include "data/data_apply.hpp"
#include "data/data_index.hpp"
#include "nnh/assets.hpp"
#include "nnh/data.hpp"
#include "nnh/detail/name_hash.hpp"

namespace nnh::detail {

namespace {

using idxfmt::Group;

Error refuse(ErrorCode code, const char* path, const char* why) {
    NNH_LOG_WARN(data, "data::load(\"%s\"): %s", path, why);
    return Error::make(code, "data \"%.24s\": %.30s", path, why);
}

const char* article(char first) {
    return first != 0 && std::strchr("aeiou", first) != nullptr ? "an" : "a";
}

ErrorCode code_of(DataProblem problem) {
    switch (problem) {
    case DataProblem::NewerVersion:
        return ErrorCode::Unsupported;
    case DataProblem::OverLimit:
        return ErrorCode::LimitReached;
    default:
        return ErrorCode::Corrupted;
    }
}

/// The index and T's entry, or the error to return.
Result<const DataTypeEntry*> prepare(TypeId type, const char* shown) {
    auto loaded = data_index_load();
    if (!loaded) return loaded.error();
    const DataTypeEntry* t = data_type(type);
    if (t == nullptr) return refuse(ErrorCode::InvalidArgument, shown, "the type has no NNH_DATA_TYPE");
    if (t->index_type < 0) return refuse(ErrorCode::NotFound, shown, "the type isn't in the build's schema");
    return t;
}

/// Index entry `entry` (a data asset) into `out`, if it is of type `t`.
Result<void> load_entry(const DataTypeEntry& t, int entry, void* out) {
    const IndexView& index = data_index();
    const char* path = index.path(entry);
    const int other = index.type_of(entry);
    if (other != t.index_type) {
        char why[64];
        const IndexType o = index.type(other);
        std::snprintf(why, sizeof why, "is %s %.*s, not %s %s", article(o.key[0]), o.key_length, o.key,
                      article(t.info->key[0]), t.info->key);
        return refuse(ErrorCode::InvalidArgument, path, why);
    }
    char asset[kMaxAssetPath + 1];
    if (std::snprintf(asset, sizeof asset, "%s.%s", path, t.info->key) >= static_cast<int>(sizeof asset)) {
        return refuse(ErrorCode::NotFound, path, "path too long");
    }
    auto read = assets::read(asset, data_scratch(), data_scratch_size());
    if (!read) {
        const ErrorCode code = read.error().code;
        if (code == ErrorCode::LimitReached)
            return refuse(ErrorCode::Corrupted, path, "larger than the index says");
        return refuse(code, path, read.error().message);
    }
    const DataCheck check =
        validate_data(data_scratch(), read.value(), data_registry(), *t.info, t.fingerprint);
    if (check.problem == DataProblem::Stale) {
        char why[64];
        std::snprintf(why, sizeof why, "converted for another version of %s: rebuild the assets",
                      t.info->name);
        NNH_LOG_WARN(data, "data::load(\"%s\"): %s", path, why);
        return Error::make(ErrorCode::Corrupted, "data for another version of %.16s: rebuild the assets",
                           t.info->name);
    }
    if (!check.ok()) return refuse(code_of(check.problem), path, check.reason);
    t.info->reset(out);
    apply_data(data_scratch(), read.value(), data_registry(), *t.info, out);
    return {};
}

} // namespace

Result<void> data_load(TypeId type, const char* path, void* out) {
    const char* shown = path != nullptr ? path : "";
    if (!engine_initialized()) return Error::make(ErrorCode::Unsupported, "data used before nnh::init()");
    if (!valid_asset_path(path)) return refuse(ErrorCode::InvalidArgument, shown, "invalid path");
    auto t = prepare(type, shown);
    if (!t) return t.error();
    const IndexView& index = data_index();
    const int entry = index.find(Group::Data, fnv1a32(path));
    if (entry < 0 || std::strcmp(index.path(entry), path) != 0) {
        return refuse(ErrorCode::NotFound, path, "no data asset of that name");
    }
    return load_entry(*t.value(), entry, out);
}

Result<void> data_load_ref(TypeId type, std::uint32_t id, void* out) {
    if (!engine_initialized()) return Error::make(ErrorCode::Unsupported, "data used before nnh::init()");
    if (id == 0) return Error::make(ErrorCode::NotFound, "data: empty reference");
    auto t = prepare(type, "(reference)");
    if (!t) return t.error();
    const int entry = data_index().find(Group::Data, id);
    if (entry < 0) {
        NNH_LOG_WARN(data, "data::load: no data asset with the reference %08X", static_cast<unsigned>(id));
        return Error::make(ErrorCode::NotFound, "data: no asset with the reference %08X",
                           static_cast<unsigned>(id));
    }
    return load_entry(*t.value(), entry, out);
}

} // namespace nnh::detail
