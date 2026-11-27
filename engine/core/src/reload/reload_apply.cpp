// The apply point of hot reload (see reload_apply.hpp): the pending batch, the counters and this frame's list behind
// FR-13's queries, the AssetsReloaded event, the log lines and the ReloadApplied/ReloadRefused records.

#include "reload/reload_apply.hpp"

#include "core/shutdown_hooks.hpp"
#include "log/engine_channels.hpp"
#include "nnh/app.hpp"
#include "nnh/assets.hpp"
#include "nnh/events.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/time.hpp"
#include "reload/reload_assets.hpp"
#include "reload/reload_batch.hpp"
#include "reload/reload_files.hpp"
#include "reload/reload_index.hpp"
#include "reload/reload_memory.hpp"
#include "reload/reload_names.hpp"
#include "reload/reload_overlay.hpp"
#include "reload/reload_receiver.hpp"

namespace nnh::detail {


namespace reload {
namespace {

using remote::ReloadReason;
using remote::ReloadStage;

ReloadFiles g_files;
ReloadReceiver g_receiver;
struct Pending {
    bool waiting = false;
    std::uint16_t batch = 0, files = 0;
};
Pending g_pending;
std::uint32_t g_count = 0;       // applied batches
std::uint32_t g_list_frame = 0;  // the frame whose batch the list holds (0: none)
const char** g_list = nullptr;   // this frame's paths (into the kept batch block)
int g_list_count = 0;
bool g_hooked = false;

void apply_hook(std::uint32_t frame) { apply_now(frame); }

void hook_shutdown() { reload_shutdown(); }

void free_list() {
    mem::free(g_list);
    g_list = nullptr;
    g_list_count = 0;
    g_list_frame = 0;
}

void make_list(std::uint32_t frame) {
    free_list();
    const int n = g_files.count();
    g_list = static_cast<const char**>(
        mem::alloc(MemRegion::Heap, reload_memory_tag(), sizeof(const char*) * static_cast<std::size_t>(n > 0 ? n : 1)));
    if (g_list == nullptr) return; // the queries then say nothing for this batch; the batch itself is applied
    for (int i = 0; i < n; ++i) g_list[i] = g_files.files()[i].path;
    g_list_count = n;
    g_list_frame = frame;
}

void log_applied([[maybe_unused]] std::uint32_t frame, [[maybe_unused]] int n) { // no INFO lines in Release
    char list[kListLimit + 1];
    format_list(g_list, g_list_count, list, sizeof list);
    NNH_LOG_INFO(assets, "reloaded %d (frame %u): %s", n, static_cast<unsigned>(frame), list);
    for (int i = 0; i < g_files.count(); ++i) {
        if (g_files.files()[i].op == remote::ReloadOp::Remove) {
            NNH_LOG_WARN(assets, "\"%s\" was removed: what was loaded from it keeps its content", g_files.files()[i].path);
        }
    }
}

void refused(std::uint16_t batch, const BatchResult& r, std::uint32_t frame) {
    const char* what = r.file != remote::kNoFile && r.file < g_files.count() ? g_files.files()[r.file].path : "batch";
    if (r.missing > 0) {
        NNH_LOG_WARN(assets, "reload refused (frame %u): %s: %s for %u bytes (old content kept)",
                     static_cast<unsigned>(frame), what, remote::reload_reason_text(r.reason),
                     static_cast<unsigned>(r.missing));
    } else {
        NNH_LOG_WARN(assets, "reload refused (frame %u): %s: %s (old content kept)", static_cast<unsigned>(frame), what,
                     remote::reload_reason_text(r.reason));
    }
    std::uint8_t out[remote::kMaxReloadRecord];
    send_reload_record(out, remote::encode_reload_refused({.batch = batch, .stage = r.stage, .reason = r.reason,
                                                           .file = r.file, .frame = frame, .missing = r.missing},
                                                          out));
}

} // namespace

ReloadFiles& session_files() { return g_files; }
ReloadReceiver& session_receiver() { return g_receiver; }

void ensure_shutdown_hook() {
    if (!g_hooked) {
        require_shutdown_hook(hook_shutdown);
        g_hooked = true;
    }
}

void schedule_apply(std::uint16_t batch, std::uint16_t files) {
    g_pending = Pending{true, batch, files};
    g_reload_apply = apply_hook;
    ensure_shutdown_hook();
}

bool apply_pending() { return g_pending.waiting; }

void apply_now(std::uint32_t frame) {
    g_reload_apply = nullptr;
    if (!g_pending.waiting) return;
    const Pending p = g_pending;
    g_pending = Pending{};
    const std::int64_t start = platform::ticks_ns();
    BatchResult r;
    if (!activate()) {
        r.stage = ReloadStage::Memory;
        r.reason = ReloadReason::NoMemory;
        r.missing = static_cast<std::uint32_t>(sizeof(OverlayEntry) * kOverlaySlots);
    } else {
        r = apply_batch(g_files.files(), g_files.count());
    }
    const auto micros = static_cast<std::uint32_t>((platform::ticks_ns() - start) / 1000);
    if (!r.ok) {
        refused(p.batch, r, frame);
        g_files.drop_batch();
        return;
    }
    ++g_count;
    g_reload_applied_frame = frame; // the UI's relayout reads this (reload_hooks.hpp): no reload code linked for it
    g_files.keep_batch();
    make_list(frame);
    log_applied(frame, g_list_count);
    events::publish(AssetsReloaded{g_count, static_cast<std::uint16_t>(r.changed), static_cast<std::uint16_t>(r.removed)});
    std::uint8_t out[remote::kMaxReloadRecord];
    send_reload_record(out, remote::encode_reload_applied({.batch = p.batch, .files = p.files, .frame = frame,
                                                           .count = g_count, .micros = micros},
                                                          out));
}

void drop_pending() {
    if (!g_pending.waiting) return;
    g_pending = Pending{};
    g_reload_apply = nullptr;
    g_files.drop_batch();
}

void report_refusal(std::uint16_t batch, ReloadStage stage, ReloadReason reason, std::uint16_t file,
                    std::uint32_t frame, const char* what) {
    NNH_LOG_WARN(assets, "reload refused (frame %u): %s: %s (old content kept)", static_cast<unsigned>(frame),
                 what != nullptr ? what : "transfer", remote::reload_reason_text(reason));
    std::uint8_t out[remote::kMaxReloadRecord];
    send_reload_record(out, remote::encode_reload_refused({.batch = batch, .stage = stage, .reason = reason,
                                                           .file = file, .frame = frame, .missing = 0},
                                                          out));
}

void reload_shutdown() {
    drop_pending();
    if (g_receiver.state() != ReloadReceiver::State::Idle) g_receiver.detach(g_files.sink());
    free_list();
    if (OverlayTable* o = overlay()) {
        o->each_file([](void*, std::uint16_t file) { ReloadFiles::remove_file(file); }, nullptr);
    }
    index_release();
    deactivate();
    g_files.release();
    ReloadFiles::wipe();
    clear_kinds();
    g_receiver = ReloadReceiver{};
    g_count = 0;
    g_hooked = false;
}

} // namespace reload

std::uint32_t reload_count_dev() { return reload::g_count; }

bool reloaded_dev(const char* path_or_name) {
    if (reload::g_list_frame == 0 || reload::g_list_frame != static_cast<std::uint32_t>(frame_count())) return false;
    for (int i = 0; i < reload::g_list_count; ++i) {
        if (reload::matches(path_or_name, reload::g_list[i])) return true;
    }
    return false;
}

assets::ReloadedPaths reloaded_paths_dev() {
    if (reload::g_list_frame == 0 || reload::g_list_frame != static_cast<std::uint32_t>(frame_count())) return {};
    return assets::ReloadedPaths{reload::g_list, reload::g_list_count};
}

} // namespace nnh::detail
