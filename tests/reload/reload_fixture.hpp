#pragma once
// Helpers for the hot reload tests (spec 031): a game with packed assets on the fake platform, an in-memory PC that
// sends batches over the remote connection, and a reader of the reload records and log lines. Batches are built here by
// hand (messages, paths, CRCs, chunks), not with the `nnh` side.

#include <cstdint>
#include <cstring>

#include "core/crc32.hpp"
#include "nnh/app.hpp"
#include "nnh/assets.hpp"
#include "nnh/build.hpp"
#include "nnh/log.hpp"
#include "nnh/platform_fake.hpp"
#include "reload/reload_hooks.hpp"
#include "reload/reload_wire.hpp"
#include "../remote/remote_fixture.hpp"
#include "test.hpp"

/// Session tests need the remote session, which only Debug/Development builds have (Release reloads nothing).
#define RELOAD_DEV_ONLY() if constexpr (!nnh::build::kDevToolsEnabled) return

namespace reload_test {

namespace wire = nnh::detail::remote;
namespace fake = nnh::platform_fake;
using remote_test::send;

/// One file of a batch to send.
struct Put {
    const char* path;
    const void* data;
    std::size_t size;
    bool remove = false;
};

inline Put text(const char* path, const char* content) { return Put{path, content, std::strlen(content)}; }
inline Put removed(const char* path) { return Put{path, nullptr, 0, true}; }

/// Sends Begin, every File (path, CRC) and its chunks; then (when `apply_frame` ≥ 0) the Apply for that frame.
inline void send_batch(std::uint16_t batch, const Put* files, int count, long apply_frame = 0) {
    std::uint32_t total = 0;
    for (int i = 0; i < count; ++i) total += static_cast<std::uint32_t>(std::strlen(files[i].path) + files[i].size);
    send(wire::reload_begin_message(batch, static_cast<std::uint16_t>(count), total));
    for (int i = 0; i < count; ++i) {
        const Put& f = files[i];
        const auto len = static_cast<std::uint8_t>(std::strlen(f.path));
        const auto op = f.remove ? wire::ReloadOp::Remove : wire::ReloadOp::Put;
        send(wire::reload_file_message(static_cast<std::uint16_t>(i), op, len, static_cast<std::uint32_t>(f.size)));
        fake::remote_push(f.path, len);
        if (f.remove) continue;
        const std::uint32_t crc = nnh::detail::crc32(f.data, f.size);
        std::uint8_t c[4];
        for (int b = 0; b < 4; ++b) c[b] = static_cast<std::uint8_t>(crc >> (8 * b));
        fake::remote_push(c, 4);
        for (std::size_t at = 0; at < f.size; at += wire::kReloadMaxChunk) {
            const std::size_t n = f.size - at < wire::kReloadMaxChunk ? f.size - at : wire::kReloadMaxChunk;
            send(wire::reload_chunk_message(static_cast<std::uint16_t>(i), static_cast<std::uint16_t>(n),
                                            static_cast<std::uint32_t>(at)));
            fake::remote_push(static_cast<const std::uint8_t*>(f.data) + at, n);
        }
    }
    if (apply_frame >= 0) {
        send(wire::reload_apply_message(batch, static_cast<std::uint16_t>(count), static_cast<std::uint32_t>(apply_frame)));
    }
}

/// A fresh engine with `count` packed assets (each listed in the generated `.nnh_assets`), connected to the PC, Hello
/// sent and answered (one frame run). The index text lives in `index` (kept alive by the caller).
inline void start(const Put* assets, int count, char* index, std::size_t index_capacity) {
    nnh::shutdown();
    fake::reset();
    std::size_t used = 0;
    index[0] = '\0';
    for (int i = 0; i < count; ++i) {
        const std::size_t n = std::strlen(assets[i].path);
        if (used + n + 2 > index_capacity) break;
        std::memcpy(index + used, assets[i].path, n);
        used += n;
        index[used++] = '\n';
        index[used] = '\0';
        fake::add_asset(assets[i].path, assets[i].data, assets[i].size);
    }
    fake::add_asset(".nnh_assets", index, used);
    fake::remote_connect();
    CHECK(nnh::init());
    nnh::detail::reload::install(); // plain files only: the tests link the reload core themselves
    send(remote_test::hello());
    CHECK(nnh::frame()); // frame 1 attaches
}

/// Runs `n` frames.
inline void frames(int n) {
    for (int i = 0; i < n; ++i) CHECK(nnh::frame());
}

/// Reads a whole asset into `out` (NUL-terminated). @return its size, or -1.
inline long read_text(const char* path, char* out, std::size_t capacity) {
    auto r = nnh::assets::read(path, out, capacity - 1);
    if (!r) {
        out[0] = '\0';
        return -1;
    }
    out[r.value()] = '\0';
    return static_cast<long>(r.value());
}

/// The reload records the console sent.
struct Records {
    int staged = 0, applied = 0, refused = 0;
    wire::ReloadAppliedRecord last_applied;
    wire::ReloadRefusedRecord last_refused;
    wire::ReloadStagedRecord last_staged;
};
inline Records records() {
    Records r;
    const remote_test::Sent s = remote_test::sent();
    for (int i = 0; i < s.count; ++i) {
        const remote_test::Record& rec = s.records[i];
        if (rec.type == wire::RecordType::ReloadStaged && wire::decode_reload_staged(rec.payload, rec.length, r.last_staged)) {
            ++r.staged;
        } else if (rec.type == wire::RecordType::ReloadApplied &&
                   wire::decode_reload_applied(rec.payload, rec.length, r.last_applied)) {
            ++r.applied;
        } else if (rec.type == wire::RecordType::ReloadRefused &&
                   wire::decode_reload_refused(rec.payload, rec.length, r.last_refused)) {
            ++r.refused;
        }
    }
    return r;
}

/// Log lines containing `needle` while alive; keeps the last one.
struct Lines {
    const char* needle;
    int count = 0;
    char last[nnh::log::kMaxLineLength] = {};

    explicit Lines(const char* n) : needle(n) { nnh::log::add_sink(&Lines::sink, this); }
    ~Lines() { nnh::log::remove_sink(&Lines::sink, this); }
    Lines(const Lines&) = delete;
    Lines& operator=(const Lines&) = delete;

    static void sink(nnh::LogLevel, const char* line, void* user) {
        auto* self = static_cast<Lines*>(user);
        if (std::strstr(line, self->needle) == nullptr) return;
        ++self->count;
        std::strncpy(self->last, line, sizeof self->last - 1);
    }
};

} // namespace reload_test
