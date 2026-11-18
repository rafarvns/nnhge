// audio: sound effects, streamed music with an intro and a loop point, crossfades, and the mixer groups
// (spec 008). Sounds come from assets/ (made by make_sounds.py).
//   A jump · B coin · X hit (hold: rapid fire, overlapping) · Y chime (Dialogue group)
//   L / R crossfade to the calm / action track · Up/Down choose a volume · Left/Right change it
//   SELECT pause the effects · START exit · SELECT + L debug overlay (frame time)
//
// Screenshot (Debug/Development): NNH_EXIT_AFTER_FRAMES=60 NNH_SCREENSHOT=build/audio.png \
//   build/pc-debug/examples/audio/audio.exe

#include <cstdio>
#include <utility>

#include <nnh/app.hpp>
#include <nnh/audio.hpp>
#include <nnh/events.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL(demo);

namespace {

using nnh::AudioGroup;
using nnh::Button;
using nnh::Screen;
namespace audio = nnh::audio;
namespace music = nnh::music;

struct Sounds {
    nnh::Sound jump, coin, hit, chime;
};

nnh::Sound load(const char* path) {
    auto result = nnh::load_sound(path);
    if (result) return std::move(result.value());
    NNH_LOG_ERROR(demo, "%s", result.error().message);
    return {};
}

/// The volume rows: master, then the three groups.
constexpr const char* kRows[] = {"Master", "Effects", "Music", "Dialogue"};
constexpr AudioGroup kGroups[] = {AudioGroup::Effects, AudioGroup::Effects, AudioGroup::Music, AudioGroup::Dialogue};

float row_volume(int row) { return row == 0 ? audio::master_volume() : audio::volume(kGroups[row]); }

void change_volume(int row, float delta) {
    float v = row_volume(row) + delta;
    v = v < 0 ? 0 : (v > 1 ? 1 : v);
    if (row == 0) {
        audio::set_master_volume(v);
    } else {
        audio::set_volume(kGroups[row], v);
    }
}

struct Counters {
    int sounds_finished = 0;
    int music_finished = 0;
};

void draw_top(int selected, const char* track, const Counters& counters) {
    using nnh::gfx::draw_text;
    draw_text(Screen::Top, {200, 8}, "NNHGE Audio", {.color = nnh::rgb(255, 210, 80), .scale = 2,
                                                     .align = nnh::TextAlign::Center, .shadow = true});
    char line[96];
    std::snprintf(line, sizeof line, "Music: %s  %.1f s%s", track, static_cast<double>(music::position()),
                  music::playing() ? "" : "  (stopped)");
    draw_text(Screen::Top, {16, 40}, line);
    draw_text(Screen::Top, {16, 54}, audio::available() ? "Audio output: yes" : "Audio output: none (silent)",
              {.color = audio::available() ? nnh::rgb(120, 220, 120) : nnh::rgb(240, 120, 120)});
    for (int row = 0; row < 4; ++row) {
        const float y = 84.0f + static_cast<float>(row) * 26.0f;
        const bool on = row == selected;
        draw_text(Screen::Top, {16, y + 4}, kRows[row], {.color = on ? nnh::colors::yellow : nnh::colors::white});
        nnh::gfx::draw_rect(Screen::Top, {110, y, 204, 16}, on ? nnh::colors::yellow : nnh::colors::gray);
        nnh::gfx::fill_rect(Screen::Top, {112, y + 2, 200 * row_volume(row), 12}, nnh::rgb(80, 160, 255));
        if (row == 1 && audio::paused(AudioGroup::Effects)) draw_text(Screen::Top, {324, y + 4}, "PAUSED");
    }
    std::snprintf(line, sizeof line, "SoundFinished: %d   MusicFinished: %d", counters.sounds_finished,
                  counters.music_finished);
    draw_text(Screen::Top, {16, 196}, line, {.color = nnh::colors::gray});
}

void draw_bottom() {
    nnh::gfx::draw_text(Screen::Bottom, {12, 16},
                        "A  jump          B  coin\n"
                        "X  hit (hold: rapid fire)\n"
                        "Y  chime (Dialogue group)\n\n"
                        "L  calm track    R  action track\n"
                        "   (2 s crossfade; the intro\n"
                        "    plays once, then it loops)\n\n"
                        "Up/Down   choose a volume\n"
                        "Left/Right  change it\n\n"
                        "SELECT  pause effects\n"
                        "START   exit\n\n"
                        "SELECT + L  frame time overlay");
}

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Audio"})) return 1;
    nnh::gfx::set_clear_color(Screen::Top, nnh::rgb(20, 24, 40));
    nnh::gfx::set_clear_color(Screen::Bottom, nnh::rgb(16, 16, 24));
    {
        Sounds sounds{load("sfx/jump"), load("sfx/coin"), load("sfx/hit"), load("sfx/chime")};
        Counters counters;
        auto on_sound = nnh::events::subscribe<nnh::SoundFinished>([&](const nnh::SoundFinished&) { ++counters.sounds_finished; });
        auto on_music = nnh::events::subscribe<nnh::MusicFinished>([&](const nnh::MusicFinished&) { ++counters.music_finished; });
        const char* track = "calm";
        if (auto played = music::play("music/calm", {.fade_in = 0.5f}); !played) {
            NNH_LOG_ERROR(demo, "%s", played.error().message);
        }
        int selected = 0;
        int rapid = 0;
        while (nnh::frame()) {
            if (nnh::input::pressed(Button::Start)) break;
            if (nnh::input::pressed(Button::A)) audio::play(sounds.jump);
            if (nnh::input::pressed(Button::B)) audio::play(sounds.coin, {.pitch = 1.0f});
            if (nnh::input::held(Button::X) && rapid++ % 5 == 0) {
                // Low priority and a varying pitch: overlapping voices, replaced when they run out.
                audio::play(sounds.hit, {.volume = 0.7f, .pitch = 0.8f + 0.1f * static_cast<float>(rapid % 5), .priority = 50});
            }
            if (!nnh::input::held(Button::X)) rapid = 0;
            if (nnh::input::pressed(Button::Y)) audio::play(sounds.chime, {.group = AudioGroup::Dialogue});
            // SELECT + L toggles the debug overlay (frame time with music playing): not a demo action.
            const bool overlay_combo = nnh::input::held(Button::Select) && nnh::input::held(Button::L);
            if (nnh::input::pressed(Button::L) && !overlay_combo) {
                track = "calm";
                (void)music::crossfade("music/calm", 2.0f);
            }
            if (nnh::input::pressed(Button::R)) {
                track = "action";
                (void)music::crossfade("music/action", 2.0f);
            }
            if (nnh::input::pressed(Button::Up)) selected = (selected + 3) % 4;
            if (nnh::input::pressed(Button::Down)) selected = (selected + 1) % 4;
            if (nnh::input::pressed(Button::Left)) change_volume(selected, -0.1f);
            if (nnh::input::pressed(Button::Right)) change_volume(selected, 0.1f);
            if (nnh::input::pressed(Button::Select) && !overlay_combo) audio::set_paused(AudioGroup::Effects, !audio::paused(AudioGroup::Effects));
            draw_top(selected, track, counters);
            draw_bottom();
        }
    } // sounds and subscriptions released here
    nnh::shutdown();
    return 0;
}
