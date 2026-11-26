#pragma once
// The engine on the fake platform with fonts in its assets (spec 029 core tests): fonts built by FontBuilder kept alive
// until the next start, log lines captured, the last coverage texture's handle.

#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "font_fixture.hpp"
#include "nnh/app.hpp"
#include "nnh/log.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace font_test {

/// Bytes the fake's assets point to (kept until the next start_engine()).
inline std::deque<std::vector<std::uint8_t>>& kept() {
    static std::deque<std::vector<std::uint8_t>> k;
    return k;
}
inline std::vector<std::uint8_t>& keep(std::vector<std::uint8_t> bytes) {
    kept().push_back(static_cast<std::vector<std::uint8_t>&&>(bytes));
    return kept().back();
}
inline std::vector<std::string>& keep_names() {
    static std::vector<std::string> names;
    return names;
}

inline nnh::GfxConfig& engine_gfx() {
    static nnh::GfxConfig gfx;
    return gfx;
}

/// The assets of the current test: (path, bytes), and the asset index listing them.
struct TestAsset {
    std::string path;
    const std::vector<std::uint8_t>* bytes = nullptr;
};
inline std::deque<TestAsset>& test_assets() {
    static std::deque<TestAsset> a;
    return a;
}
inline std::string& asset_index() {
    static std::string index;
    return index;
}

/// The engine restarted on the fake platform with the current test's assets (the fake's asset index is read at init).
inline void remount() {
    nnh::shutdown();
    nnh::platform_fake::reset();
    asset_index().clear();
    for (const TestAsset& a : test_assets()) asset_index() += a.path + "\n";
    if (!test_assets().empty()) nnh::platform_fake::add_asset(".nnh_assets", asset_index().data(), asset_index().size());
    for (const TestAsset& a : test_assets()) nnh::platform_fake::add_asset(a.path.c_str(), a.bytes->data(), a.bytes->size());
    nnh::AppConfig config;
    config.gfx = engine_gfx();
    CHECK(nnh::init(config));
}

/// A fresh engine on the fake platform (fonts and assets of earlier tests forgotten).
inline void start_engine(nnh::GfxConfig gfx = {}) {
    test_assets().clear();
    kept().clear();
    keep_names().clear();
    engine_gfx() = gfx;
    remount();
}

/// Adds an asset and restarts the engine so it sees it: add every font a test needs before it loads any.
inline void add_asset_bytes(const char* asset, const std::vector<std::uint8_t>& bytes) {
    test_assets().push_back({asset, &bytes});
    remount();
}

/// Adds `<path>.nnhfont` made by `b`.
inline void add_font(const char* path, const FontBuilder& b) {
    add_asset_bytes((std::string(path) + ".nnhfont").c_str(), keep(b.build()));
}

/// Textures the engine holds by itself (the built-in font's).
inline int builtin_textures() { return 1; }

/// The handle of the newest coverage texture.
inline nnh::platform::TextureHandle last_alpha_handle() { return nnh::platform_fake::state().last_alpha_texture; }

/// Log lines of a level while alive.
struct LogLines {
    nnh::LogLevel level;
    std::vector<std::string> lines;
    explicit LogLines(nnh::LogLevel l) : level(l) { nnh::log::add_sink(&LogLines::sink, this); }
    ~LogLines() { nnh::log::remove_sink(&LogLines::sink, this); }
    LogLines(const LogLines&) = delete;
    LogLines& operator=(const LogLines&) = delete;
    static void sink(nnh::LogLevel l, const char* line, void* user) {
        auto* self = static_cast<LogLines*>(user);
        if (l == self->level) self->lines.emplace_back(line);
    }
    bool contains(const char* part) const {
        for (const std::string& l : lines)
            if (l.find(part) != std::string::npos) return true;
        return false;
    }
    int count(const char* part) const {
        int n = 0;
        for (const std::string& l : lines) n += l.find(part) != std::string::npos ? 1 : 0;
        return n;
    }
};

} // namespace font_test
