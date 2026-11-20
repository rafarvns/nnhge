// NNH Link (spec 011): pick a PC running `nnh serve`, pair once, pick a game; the PC builds it, sends it,
// and the console starts it (returning here when it exits). Guide: docs/guides/nnh-link.md.
//
// NNH_LINK_DEMO=<screen> shows a made-up server instead of the network, and saves nothing (demo.hpp).

#include <cstdlib>

#include <nnh/app.hpp>
#include <nnh/fs.hpp>
#include <nnh/scene.hpp>

#include "demo.hpp"
#include "device.hpp"
#include "link_scene.hpp"
#include "net_client.hpp"
#include "platform_link.hpp"
#include "storage_file.hpp"

using namespace link_app;

namespace {

void run_demo(const char* screen) {
    Storage storage; // made up: nothing is read or saved
    DemoPorts ports(screen);
    LinkState link(ports, ports, ports, storage);
    ports.advance(link);
    LinkScene scene(link, storage, false);
    nnh::run(scene);
}

void run_link(const char* argv0) {
    Storage storage;
    load_storage(storage);
    // The sockets live in this block: they close before the network stops.
    NetFinder finder;
    NetClient client(storage, platform::fill_random);
    SdDevice device(nnh::fs::data_root(), self_path(argv0));
    LinkState link(finder, client, device, storage);
    link.start();
    LinkScene scene(link, storage, true);
    nnh::run(scene);
    client.cancel();
    save_storage(storage);
}

} // namespace

int main(int argc, char** argv) {
    // Data folder "nnh": link.sav, and the received games (sdmc:/3ds/nnh/<game>.3dsx on the 3DS).
    if (!nnh::init({.data_folder = "nnh", .gfx = {.max_draws = 4096}})) return 1;
    if (const char* demo = std::getenv("NNH_LINK_DEMO"); demo != nullptr) {
        run_demo(demo);
    } else {
        const bool network = platform::start_network();
        run_link(argc > 0 ? argv[0] : nullptr);
        if (network) platform::stop_network();
    }
    nnh::shutdown(); // a game chosen on the console starts now (launch_after_exit)
    return 0;
}
