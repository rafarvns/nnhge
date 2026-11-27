// load_sprite_library (spec 016 FR-16, FR-18; T6.2): asset `<path>.nnhsprite` → validate → copy into a block with the
// options' extra capacities.

#include <cstdio>
#include <cstring>

#include "animation/sprite_copy.hpp"
#include "animation/sprite_library_internal.hpp"
#include "animation/sprite_load.hpp"
#include "animation/sprite_validate.hpp"
#include "assets/asset_path.hpp"
#include "nnh/assets.hpp"
#include "nnh/build.hpp"
#include "nnh/profile.hpp"

namespace nnh {

namespace {

using namespace detail::spritefmt;

ErrorCode code_of(detail::SpriteProblem problem) {
    switch (problem) {
    case detail::SpriteProblem::NewerVersion: return ErrorCode::Unsupported;
    case detail::SpriteProblem::OverLimit: return ErrorCode::LimitReached;
    default: return ErrorCode::Corrupted;
    }
}

Error refuse(ErrorCode code, const char* name, const char* what) {
    NNH_LOG_WARN(animation, "load_sprite_library(\"%s\"): %s", name, what);
    return Error::make(code, "sprite library: %s", what);
}

} // namespace

Result<SpriteLibrary> detail::sprite_library_from_file(const void* data, std::size_t size,
                                                       const LoadSpriteLibraryOptions& o, const char* name) {
    const int extras[kSectionCount] = {o.extra_frames, o.extra_clips,  o.extra_clip_frames,
                                       o.extra_markers, o.extra_states, o.extra_transitions};
    auto block = sprite_block_from_file(data, size, extras, name);
    if (!block) return block.error();
    return SpriteLibraryAccess::adopt(block.value());
}

Result<detail::SpriteBlock*> detail::sprite_block_from_file(const void* data, std::size_t size,
                                                            const int (&extras)[6], const char* name) {
    const SpriteCheck check = validate_sprite_library(data, size);
    if (!check.ok()) return refuse(code_of(check.problem), name, check.reason);
    Header file;
    std::memcpy(&file, data, sizeof file);
    SpriteCapacities caps;
    bool limit = false;
    if (const char* why = sprite_loaded_capacities(file, extras, caps, limit)) {
        return refuse(limit ? ErrorCode::LimitReached : ErrorCode::InvalidArgument, name, why);
    }
    const std::uint32_t bytes = sprite_layout(caps).total;
    void* memory = mem::alloc(MemRegion::Heap, animation_memory_tag(), bytes, 8);
    if (memory == nullptr) return refuse(ErrorCode::OutOfMemory, name, "no memory for the library");
    std::memset(memory, 0, bytes);
    copy_sprite_library(static_cast<const std::uint8_t*>(data), caps, static_cast<std::uint8_t*>(memory));
    return static_cast<SpriteBlock*>(memory);
}

Result<SpriteLibrary> load_sprite_library(const char* path, const LoadSpriteLibraryOptions& options) {
    NNH_PROFILE_SCOPE("load_sprite_library");
    if (path == nullptr) return Error::make(ErrorCode::InvalidArgument, "load_sprite_library(nullptr)");
    char asset[detail::kMaxAssetPath + 1];
    if (std::snprintf(asset, sizeof asset, "%s.nnhsprite", path) >= static_cast<int>(sizeof asset)) {
        return refuse(ErrorCode::InvalidArgument, path, "path too long");
    }
    auto file = assets::load(asset);
    if (!file) return file.error();
    auto library = detail::sprite_library_from_file(file.value().data(), file.value().size(), options, path);
    if constexpr (build::kDevToolsEnabled) { // spec 031: the library reloads with its asset
        if (library) detail::sprite_reload_loaded(SpriteLibraryAccess::block(library.value()), asset, path, options);
    }
    return library;
}

} // namespace nnh
