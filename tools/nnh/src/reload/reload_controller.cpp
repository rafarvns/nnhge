// The reload cycle on the PC (see reload_controller.hpp).

#include "reload/reload_controller.hpp"

#include <cstdio>

#include "reload/batch_sender.hpp"
#include "reload/reload_names.hpp"
#include "reload/reload_wire.hpp"

namespace nnh::tool::reload {
namespace {

namespace wire = nnh::detail::remote;

std::vector<std::uint8_t> read_file(const std::string& path, bool& ok) {
    std::vector<std::uint8_t> out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    ok = f != nullptr;
    if (f == nullptr) return out;
    std::uint8_t buffer[64 * 1024];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) out.insert(out.end(), buffer, buffer + n);
    ok = std::ferror(f) == 0;
    std::fclose(f);
    return out;
}

std::string seconds(long long ms) {
    char text[32];
    std::snprintf(text, sizeof text, "%.2f s", static_cast<double>(ms) / 1000.0);
    return text;
}

std::string kb(std::size_t bytes) {
    char text[32];
    std::snprintf(text, sizeof text, "%.1f KB", static_cast<double>(bytes) / 1024.0);
    return text;
}

std::string names(const std::vector<std::string>& paths) {
    std::vector<const char*> p;
    for (const std::string& s : paths) p.push_back(s.c_str());
    char out[nnh::detail::reload::kListLimit + 1];
    nnh::detail::reload::format_list(p.data(), static_cast<int>(p.size()), out, sizeof out);
    return out;
}

} // namespace

ReloadController::ReloadController(ControllerSetup setup) : setup_(std::move(setup)) {}

void ReloadController::start() {
    mirror_.scan(setup_.out_dir);
    watcher_.reset(list_sources(setup_.src_dir));
    last_poll_ = setup_.now_ms();
    for (const std::string& c : mirror_.hash_collisions()) note("reload: " + c + " share a path hash: they reload together");
}

void ReloadController::poll() {
    if (!enabled_ || state_ != State::Idle) return;
    const long long now = setup_.now_ms();
    if (requested_) {
        requested_ = false;
        saved_at_ = now;
        run_cycle(0);
        return;
    }
    if (!watching_ || now - last_poll_ < kPollMs) return;
    last_poll_ = now;
    const bool was_gathering = watcher_.gathering();
    if (watcher_.poll(list_sources(setup_.src_dir), now)) run_cycle(0);
    else if (!was_gathering && watcher_.gathering()) saved_at_ = now;
}

void ReloadController::request(const std::string& output_path) {
    if (!output_path.empty()) mirror_.forget(output_path);
    requested_ = true;
}

bool ReloadController::inside_assets(const std::string& game_path) const {
    if (setup_.game_dir.empty()) return false;
    const std::string full = setup_.game_dir + "/" + game_path;
    return full.size() > setup_.src_dir.size() && full.compare(0, setup_.src_dir.size(), setup_.src_dir) == 0 &&
           full[setup_.src_dir.size()] == '/';
}

bool ReloadController::run_cycle(std::uint32_t apply_frame, const std::vector<std::uint8_t>& tail,
                                 const std::string& script_source) {
    outcome_ = Outcome{};
    const bool inside = script_source.empty() || inside_assets(script_source);
    const std::string outside_source = inside ? std::string() : setup_.game_dir + "/" + script_source;
    if (!enabled_) {
        outcome_.done = true;
        outcome_.why = "the game has no hot reload";
        set_pending({}, tail);
        return false;
    }
    if (saved_at_ == 0) saved_at_ = setup_.now_ms();
    const BuildOutcome built = setup_.build(outside_source);
    for (const std::string& line : built.lines) note(line);
    std::string first_source = watcher_.changed().empty() ? std::string("assets") : watcher_.changed()[0];
    if (!script_source.empty()) { // a script's swap: the file it swapped, relative to the assets folder when inside
        first_source = inside ? (setup_.game_dir + "/" + script_source).substr(setup_.src_dir.size() + 1) : script_source;
    }
    if (!built.ok || !built.restart.empty()) {
        const bool restart = built.ok;
        const std::string why = restart ? "reload refused: " + built.restart + ": rebuild and restart (nnh run)"
                                        : "reload refused: conversion failed (see the lines above)";
        note(why);
        set_pending(encode_notice(restart, first_source, 0), tail);
        outcome_ = Outcome{true, false, false, 0, {}, restart ? "needs a restart" : "conversion failed"};
        state_ = State::Idle;
        saved_at_ = 0;
        watcher_.reset(list_sources(setup_.src_dir));
        return false;
    }
    changes_ = mirror_.diff(setup_.out_dir);
    watcher_.reset(list_sources(setup_.src_dir));
    if (changes_.empty()) {
        note("reload: nothing changed in the converted assets");
        set_pending({}, tail);
        outcome_.done = true;
        saved_at_ = 0;
        return false;
    }
    std::vector<BatchFileData> files;
    for (const OutputChange& c : changes_) {
        BatchFileData f{c.path, c.removed, {}};
        bool ok = true;
        if (!c.removed) f.bytes = read_file(setup_.out_dir + "/" + c.path, ok);
        if (!ok) {
            note("reload: can't read " + c.path + ": try again");
            outcome_.done = true;
            set_pending({}, tail);
            return false;
        }
        files.push_back(std::move(f));
    }
    batch_ = static_cast<std::uint16_t>(batch_ == 0xFFFF ? 1 : batch_ + 1);
    const EncodedBatch e = encode_batch(batch_, files);
    if (!e.error.empty()) {
        note("reload refused: " + e.error);
        outcome_ = Outcome{true, false, false, 0, {}, e.error};
        set_pending({}, tail);
        return false;
    }
    std::vector<std::uint8_t> batch = e.bytes;
    const std::vector<std::uint8_t> apply = encode_apply(batch_, e.files, apply_frame, seq_++);
    batch.insert(batch.end(), apply.begin(), apply.end());
    set_pending(batch, tail);
    for (const OutputChange& c : changes_) outcome_.paths.push_back(c.path);
    outcome_.sent = true;
    note("reload " + std::to_string(batch_) + ": " + std::to_string(e.files) + " files, " + kb(e.total) + " (build " +
         seconds(built.ms) + ")");
    state_ = State::Sending;
    return true;
}

void ReloadController::set_pending(const std::vector<std::uint8_t>& batch, const std::vector<std::uint8_t>& tail) {
    pending_.clear();
    pending_.append(batch);
    pending_.mark_tail();
    pending_.append(tail);
    allowance_ = kInitialAllowance;
}

std::vector<std::uint8_t> ReloadController::take_bytes() {
    if (pending_.empty()) return {};
    std::vector<std::uint8_t> out = pending_.take(allowance_);
    if (pending_.empty() && state_ == State::Sending) state_ = State::Waiting;
    return out;
}

void ReloadController::finish_cycle() {
    state_ = State::Idle;
    saved_at_ = 0;
    outcome_.done = true;
}

void ReloadController::on_record(const remote::Record& r) {
    switch (r.type) {
    case wire::RecordType::Tick:
        if (!pending_.empty()) allowance_ += kAllowancePerTick;
        return;
    case wire::RecordType::Rejected:
        if (state_ != State::Idle && r.reason == wire::Reject::Type) {
            enabled_ = false;
            pending_.drop_batch(); // a script's Resume still goes out
            note("the game has no hot reload (built before spec 031): watching off");
            outcome_.why = "the game has no hot reload";
            finish_cycle();
        }
        return;
    case wire::RecordType::ReloadApplied: {
        wire::ReloadAppliedRecord a;
        if (!wire::decode_reload_applied(r.payload.data(), static_cast<std::uint32_t>(r.payload.size()), a)) return;
        if (a.batch != batch_ || state_ == State::Idle) return;
        mirror_.accept(changes_);
        outcome_.applied = true;
        outcome_.frame = a.frame;
        const long long after = saved_at_ != 0 ? setup_.now_ms() - saved_at_ : 0;
        note("reload " + std::to_string(a.batch) + " applied at frame " + std::to_string(a.frame) + ", " +
             seconds(after) + " after the save: " + names(outcome_.paths));
        finish_cycle();
        return;
    }
    case wire::RecordType::ReloadRefused: {
        wire::ReloadRefusedRecord f;
        if (!wire::decode_reload_refused(r.payload.data(), static_cast<std::uint32_t>(r.payload.size()), f)) return;
        if (f.batch == 0) return; // the echo of our own notice (a batch's own refusal, Restart included, has its number)
        if (f.batch != batch_ || state_ == State::Idle) return;
        std::string what = f.file < changes_.size() ? changes_[f.file].path + ": " : std::string();
        outcome_.why = what + wire::reload_reason_text(f.reason);
        pending_.drop_batch(); // the rest of the batch, never half a unit; a script's Resume still goes out
        note("reload " + std::to_string(f.batch) + " refused by the game: " + outcome_.why);
        finish_cycle();
        return;
    }
    default: return;
    }
}

std::vector<std::string> ReloadController::take_notes() {
    std::vector<std::string> out;
    out.swap(notes_);
    return out;
}

} // namespace nnh::tool::reload
