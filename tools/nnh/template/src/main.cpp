// The game's entry point: start the engine, run the first scene until the scene stack is empty, shut down.
// NNHGE docs: api/app.md (init, run), api/scene.md (scenes).

#include "scenes/play_scene.hpp"

#include <nnh/app.hpp>
#include <nnh/scene.hpp>

int main() {
    // The window title and the data folder come from TITLE in CMakeLists.txt (api/app.md).
    if (!nnh::init({})) return 1;

    // One scene; it pops itself when the player quits, and the empty stack ends run() (api/scene.md).
    PlayScene play;
    nnh::run(play);

    nnh::shutdown();
    return 0;
}
