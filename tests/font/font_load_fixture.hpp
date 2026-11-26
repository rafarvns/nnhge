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

/// A fresh engine on the fake platform (fonts and assets of earlier tests forgotten).
inline void start_engine(nnh::GfxConfig gfx = {}) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    kept().clear();
    keep_names().clear();
    keep_names().reserve(64);
    nnh::AppConfig config;
    config.gfx = gfx;
    CHECK(nnh::init(config));
}

/// Adds `<path>.nnhfont` made by `b` to the fake's assets.
inline void add_font(const char* path, const FontBuilder& b) {
    std::vector<std::uint8_t>& bytes = keep(b.build());
    keep_names().push_back(std::string(path) + ".nnhfont");
    nnh::platform_fake::add_asset(keep_names().back().c_str(), bytes.data(), bytes.size());
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
