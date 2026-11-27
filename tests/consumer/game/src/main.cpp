// nnh_consumer: the consumer test project's game (spec 032 FR-16), built from a game project outside the engine's own
// build. It loads one asset of each kind it ships and logs "consumer: PASS <crc32>", the same line on the PC (every
// way the engine came in, every flavour) and on the 3DS. START quits.
//   NNH_EXIT_AFTER_FRAMES=10 build/pc-debug/game/nnh_consumer.exe
//   nnh run nnh_consumer [--pc] --script game/tests/check.nnhs

#include <memory>
#include <new>
#include <nnh/app.hpp>

#include "check_scene.hpp"

int main() {
    nnh::AppConfig config; // the title: TITLE in game/CMakeLists.txt
    config.gfx.max_draws_3d = 8; // meshes need 3D on
    if (!nnh::init(config)) return 1;
    {
        std::unique_ptr<CheckScene> scene(new (std::nothrow) CheckScene());
        if (scene != nullptr) nnh::run(*scene);
    } // textures and sounds go before the engine
    nnh::shutdown();
    return 0;
}
