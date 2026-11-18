// scenes: a title, a level and a pause menu, glued by the scene stack and the event bus.
//   Title --A--> Level --START--> Pause (pushed over the level) --START--> Level --B--> Title --START--> quit
// The score is updated through a CoinCollected event (level publishes, Score listens).
//
// SCENES_DEMO_START=level|pause starts further in (used for the documentation screenshots).

#include <cstdlib>
#include <cstring>

#include <nnh/app.hpp>
#include <nnh/events.hpp>
#include <nnh/gfx.hpp>
#include <nnh/scene.hpp>

#include "game_events.hpp"
#include "level_scene.hpp"
#include "pause_scene.hpp"
#include "title_scene.hpp"

int main() {
    if (!nnh::init({.title = "NNHGE Scenes"})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(24, 24, 24));

    // The game creates and owns its scenes; they outlive the stack.
    TitleScene title;
    PauseScene pause;
    LevelScene level(pause, title);
    title.set_level(level);

    const char* start = std::getenv("SCENES_DEMO_START");
    if (start != nullptr && std::strcmp(start, "pause") == 0) {
        nnh::scenes::push(level);
        nnh::events::publish(CoinCollected{30}); // queued before the level subscribes: it still receives it
        nnh::run(pause);                         // pushed over the level
    } else if (start != nullptr && std::strcmp(start, "level") == 0) {
        nnh::events::publish(CoinCollected{30});
        nnh::run(level);
    } else {
        nnh::run(title); // until the stack is empty (START on the title) or the system closes the game
    }

    nnh::shutdown();
    return 0;
}
