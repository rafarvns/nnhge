// The runner's side of hot reload (spec 031 FR-1, FR-15, FR-22): in interactive mode with a reload controller, the game
// is attached at once (not exclusive: its own input still counts) so a save can reach it; typed `reload [PATH]`; the
// controller's paced bytes go out after the frame messages; its records and notes pass through.

#include <cctype>

#include "reload/reload_controller.hpp"
#include "reload/reload_wire.hpp"
#include "remote/script_runner.hpp"

namespace nnh::tool::remote {

void ScriptRunner::set_reload(reload::ReloadController* controller) { reload_ = controller; }

void ScriptRunner::poll_reload() {
    if (reload_ == nullptr || closed_) return;
    if (options_.interactive && reload_->watching() && !hello_sent_) send_hello(); // attach now: a save may come
    if (attached_) reload_->poll();
    for (std::string& n : reload_->take_notes()) notes_.push_back(std::move(n));
}

std::vector<std::uint8_t> ScriptRunner::take_bytes() {
    if (reload_ == nullptr || !attached_ || closed_) return {};
    return reload_->take_bytes();
}

bool ScriptRunner::typed_reload(const std::string& text) {
    std::size_t at = 0;
    while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at])) != 0) ++at;
    for (const char* script_only : {"swap-asset", "restore-asset", "expect-reload"}) {
        if (text.compare(at, std::string(script_only).size(), script_only) == 0) {
            notes_.push_back(std::string(script_only) + " is for scripts (--script); save the file instead");
            return true;
        }
    }
    if (text.compare(at, 6, "reload") != 0) return false;
    std::size_t end = at + 6;
    if (end < text.size() && std::isspace(static_cast<unsigned char>(text[end])) == 0) return false; // reloadx
    while (end < text.size() && std::isspace(static_cast<unsigned char>(text[end])) != 0) ++end;
    std::string path = text.substr(end);
    while (!path.empty() && std::isspace(static_cast<unsigned char>(path.back())) != 0) path.pop_back();
    if (reload_ == nullptr || !reload_->enabled()) {
        notes_.push_back("reload: not available (a Release build, --no-reload, or a game built before spec 031)");
        return true;
    }
    if (!hello_sent_) send_hello();
    reload_->request(path);
    return true;
}

void ScriptRunner::reload_record(const Record& record) {
    if (reload_ == nullptr) return;
    reload_->on_record(record);
    for (std::string& n : reload_->take_notes()) notes_.push_back(std::move(n));
    if (!options_.interactive) reload_outcome(); // spec 031: a script's swap waits for it
}

} // namespace nnh::tool::remote
