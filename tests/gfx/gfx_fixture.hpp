#pragma once
// Shared helpers for the 2D graphics tests: a fresh engine on the fake platform with test images.
// The images live in test_images.cpp (the fake platform reads "FAKE" + width + height).

#include "nnh/app.hpp"
#include "nnh/gfx.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/texture.hpp"
#include "test.hpp"

namespace gfx_test {

/// Shuts down any previous engine, resets the fake platform and starts the engine with `config`.
inline void restart(const nnh::AppConfig& config = {}) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init(config));
}

/// Loads a test image that must exist ("hero" 64x32, "tile" 16x16).
inline nnh::Texture load(const char* name) {
    auto result = nnh::load_texture(name);
    CHECK(result.ok());
    return result ? std::move(result.value()) : nnh::Texture{};
}

} // namespace gfx_test
