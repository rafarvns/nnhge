// foundation: the core foundation services in one small program (spec 002).
//
// - logs on its own channel (PC terminal, 3dslink -s, log.txt)
// - the debug overlay (SELECT + L or F1), with a profiled scope
// - a save file that counts how many times the program ran (survives restarts and power loss)
// - a deliberate crash: hold L + R and press A (or set NNH_DEMO_CRASH_AT_FRAME=<n> on PC for automation)

#include <cstdint>
#include <cstdlib>

#include <nnh/app.hpp>
#include <nnh/assert.hpp>
#include <nnh/debug.hpp>
#include <nnh/fs.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>
#include <nnh/profile.hpp>
#include <nnh/system.hpp>
#include <nnh/time.hpp>

NNH_LOG_CHANNEL(demo);

namespace {

struct Progress {
    std::uint32_t runs = 0;
};

constexpr const char* kSaveFile = "progress.sav";
constexpr std::uint16_t kSaveVersion = 1;

Progress load_and_count_run(const char*& status) {
    Progress progress;
    auto loaded = nnh::save::read(kSaveFile, &progress, sizeof progress, nullptr);
    if (loaded) {
        status = "loaded";
    } else if (loaded.error().code == nnh::ErrorCode::NotFound) {
        status = "new save";
    } else {
        status = "save damaged, starting over";
        NNH_LOG_WARN(demo, "could not load %s: %s", kSaveFile, loaded.error().message);
    }
    ++progress.runs;
    if (auto saved = nnh::save::write(kSaveFile, &progress, sizeof progress, kSaveVersion); !saved) {
        NNH_LOG_ERROR(demo, "could not save: %s", saved.error().message);
    }
    return progress;
}

void busy_work() {
    NNH_PROFILE_SCOPE("busy_work");
    volatile std::uint32_t x = 0;
    for (int i = 0; i < 20000; ++i) x = x + static_cast<std::uint32_t>(i);
}

std::uint64_t crash_frame_from_env() {
    const char* value = std::getenv("NNH_DEMO_CRASH_AT_FRAME");
    return value != nullptr ? std::strtoull(value, nullptr, 10) : 0;
}

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Foundation"})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(24, 40, 64));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(20, 20, 20));

    const char* save_status = "";
    const Progress progress = load_and_count_run(save_status);
    const nnh::SystemInfo system = nnh::system::info();
    NNH_LOG_INFO(demo, "run #%u on %s (%s), data in %s", static_cast<unsigned>(progress.runs), system.name,
                 system.model, nnh::fs::data_root());
    nnh::debug::set_overlay_visible(true);

    const std::uint64_t crash_at = crash_frame_from_env();
    const int line = nnh::debug::line_height();
    while (nnh::frame()) {
        busy_work();
        const bool crash_combo = nnh::input::held(nnh::Button::L) && nnh::input::held(nnh::Button::R) &&
                                 nnh::input::pressed(nnh::Button::A);
        NNH_VERIFY(!crash_combo && nnh::frame_count() != crash_at, "deliberate crash from the demo (frame %llu)",
                   static_cast<unsigned long long>(nnh::frame_count()));
        if (nnh::input::pressed(nnh::Button::Start)) break;

        nnh::debug::print(nnh::Screen::Top, 8, 8, "NNHGE core foundation", nnh::colors::yellow);
        nnh::debug::printf(nnh::Screen::Top, 8, 8 + line, "run #%u (%s)", static_cast<unsigned>(progress.runs),
                           save_status);
        nnh::debug::printf(nnh::Screen::Top, 8, 8 + 2 * line, "%s - %s - %s build", system.name, system.model,
                           nnh::build::flavor_name());
        nnh::debug::printf(nnh::Screen::Top, 8, 8 + 3 * line, "time %.1f s", nnh::time::now().seconds());
        nnh::debug::print(nnh::Screen::Top, 8, 8 + 5 * line, "SELECT+L / F1: overlay");
        nnh::debug::print(nnh::Screen::Top, 8, 8 + 6 * line, "L+R+A: deliberate crash");
        nnh::debug::print(nnh::Screen::Top, 8, 8 + 7 * line, "START: exit");
    }
    nnh::shutdown();
    return 0;
}
