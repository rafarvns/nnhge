// The data index at run time (data_index.hpp).

#include "data/data_index.hpp"

#include <cstring>

#include "core/engine_state.hpp"
#include "core/shutdown_hooks.hpp"
#include "data/schema_fingerprint.hpp"
#include "nnh/assets.hpp"
#include "nnh/platform/assets.hpp"

namespace nnh {

NNH_LOG_CHANNEL(data);
NNH_MEM_TAG(data);

namespace detail {

namespace {

struct State {
    std::uint8_t* block = nullptr; ///< the index file, then the scratch buffer
    std::size_t index_size = 0;
    std::size_t scratch_size = 0;
    IndexView view;
    DataTypeEntry types[datafmt::kMaxDataTypes];
    int type_count = 0;
};

State g_state;

void forget() {
    mem::free(g_state.block);
    g_state = State{};
}

/// The game's compiled data types, each matched with the index; warns about those the build didn't see.
void match_types() {
    const DataRegistry& registry = data_registry();
    for (const TypeNode* n = registry.types; n != nullptr; n = n->next) {
        const TypeInfo& t = *n->info;
        if (t.key[0] == '\0') continue;
        if (g_state.type_count == datafmt::kMaxDataTypes) {
            NNH_LOG_WARN(data, "more than %d data types: '%s' ignored", datafmt::kMaxDataTypes, t.key);
            continue;
        }
        DataTypeEntry& e = g_state.types[g_state.type_count++];
        e.info = &t;
        e.fingerprint = schema_fingerprint(registry, t);
        e.index_type = g_state.view.find_type(t.key);
        if (e.index_type < 0) {
            NNH_LOG_WARN(data, "type '%s' is not in the build's schema: list its source in DATA_SCHEMA",
                         t.key);
        }
    }
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

Error refuse(ErrorCode code, const char* why) {
    NNH_LOG_WARN(data, "data index %s: %s", idxfmt::kAssetName, why);
    return Error::make(code, "data index: %s", why);
}

bool read_header(std::uint8_t (&header)[idxfmt::kHeaderSize]) {
    platform::AssetStream stream = 0;
    if (!platform::asset_open(idxfmt::kAssetName, stream)) return false;
    std::size_t read = 0;
    const bool ok = platform::asset_read_at(stream, 0, header, sizeof header, read) && read == sizeof header;
    platform::asset_close(stream);
    return ok;
}

/// Reads and validates the index into a new block (freed on any error).
Result<void> read_index() {
    auto size = assets::size(idxfmt::kAssetName);
    if (!size) {
        if (size.error().code == ErrorCode::NotFound) {
            return refuse(ErrorCode::NotFound, "missing: the game has no data assets (DATA_SCHEMA)");
        }
        return refuse(size.error().code, size.error().message);
    }
    if (size.value() < idxfmt::kHeaderSize || size.value() > idxfmt::kMaxFileBytes) {
        return refuse(size.value() < idxfmt::kHeaderSize ? ErrorCode::Corrupted : ErrorCode::LimitReached,
                      "wrong size");
    }
    // The scratch size is in the header, read first so the index and the scratch share one block; the
    // validator checks the value again with the whole file.
    std::uint8_t header[idxfmt::kHeaderSize];
    if (!read_header(header)) return refuse(ErrorCode::IoError, "can't be read");
    const std::size_t scratch = header[idxfmt::kLargestAt] | (header[idxfmt::kLargestAt + 1] << 8) |
                                (static_cast<std::size_t>(header[idxfmt::kLargestAt + 2]) << 16) |
                                (static_cast<std::size_t>(header[idxfmt::kLargestAt + 3]) << 24);
    if (scratch > datafmt::kMaxFileBytes)
        return refuse(ErrorCode::Corrupted, "largest data asset over 64 KiB");
    const std::size_t total = size.value() + scratch;
    auto* block =
        static_cast<std::uint8_t*>(mem::alloc(MemRegion::Heap, data_memory_tag(), total > 0 ? total : 1));
    if (block == nullptr) return refuse(ErrorCode::OutOfMemory, "no memory");
    auto read = assets::read(idxfmt::kAssetName, block, size.value());
    const DataCheck check = read ? validate_index(block, size.value()) : DataCheck{};
    if (!read || !check.ok()) {
        mem::free(block);
        return read ? refuse(code_of(check.problem), check.reason)
                    : refuse(read.error().code, "can't be read");
    }
    g_state.block = block;
    g_state.index_size = size.value();
    g_state.scratch_size = scratch;
    g_state.view = IndexView{block};
    return {};
}

} // namespace

MemTag& data_memory_tag() {
    return NNH_MEM_TAG_REF(data);
}

Result<void> data_index_load() {
    if (!engine_initialized()) return Error::make(ErrorCode::Unsupported, "data used before nnh::init()");
    if (g_state.block != nullptr) return {};
    auto read = read_index();
    if (!read) return read.error();
    match_types();
    (void)add_shutdown_hook(data_index_release); // 8 hooks for a handful of optional modules: always room
    return {};
}

bool data_index_loaded() {
    return g_state.block != nullptr;
}

void data_index_release() {
    forget();
}

const IndexView& data_index() {
    return g_state.view;
}
std::uint8_t* data_scratch() {
    return g_state.block + g_state.index_size;
}
std::size_t data_scratch_size() {
    return g_state.scratch_size;
}

const DataTypeEntry* data_type(TypeId type) {
    for (int i = 0; i < g_state.type_count; ++i) {
        if (g_state.types[i].info->tag == type) return &g_state.types[i];
    }
    return nullptr;
}

} // namespace detail
} // namespace nnh
