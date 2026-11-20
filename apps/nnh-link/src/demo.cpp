#include "demo.hpp"

namespace link_app {

DemoPorts::DemoPorts(const char* screen) : screen_(screen) {}

bool DemoPorts::next(Server& out) {
    static const Server kServers[] = {
        {{{192, 168, 10, 53}}, {{1}}, "RAFA"},
        {{{192, 168, 10, 70}}, {{2}}, "studio-laptop"},
    };
    if (!asked_ || answered_ >= 2) return false;
    out = kServers[answered_++];
    return true;
}

bool DemoPorts::connect(nnh::link::Ipv4) {
    state_ = screen_ == "code" ? State::NeedCode : State::Ready;
    return true;
}

bool DemoPorts::list_games() {
    games_ = {{"audio", "NNHGE Audio"}, {"foundation", "NNHGE Foundation"}, {"hello", "Hello NNHGE"},
              {"input", "NNHGE Input"}, {"scenes", "NNHGE Scenes"}, {"sprites", "NNHGE Sprites"},
              {"text", "NNHGE Text"}};
    return true;
}

bool DemoPorts::build(const nnh::link::BuildRequest&, nnh::link::FileSink&) {
    lines_ = {"[1/4] Building CXX object examples/audio/CMakeFiles/audio.dir/main.cpp.o",
              "[2/4] Linking CXX executable examples/audio/audio.elf",
              "[3/4] Generating audio.smdh", "[4/4] Generating audio.3dsx"};
    build_ = Build::Building;
    if (screen_ == "receiving") build_ = Build::Receiving;
    if (screen_ == "done") build_ = Build::Received;
    if (screen_ == "failed") {
        lines_ = {"FAILED: examples/audio/CMakeFiles/audio.dir/main.cpp.o",
                  "examples/audio/main.cpp:42:5: error: 'plya' is not a member of 'nnh::audio'",
                  "   42 |     nnh::audio::plya(jump);", "      |     ^~~~~~~~~~~~~~~~",
                  "ninja: build stopped: subcommand failed."};
        build_ = Build::Failed;
    }
    return true;
}

void DemoPorts::advance(LinkState& link) {
    link.start();
    link.tick();
    if (screen_ == "servers") return;
    if (screen_ == "address") {
        link.press(Key::Up); // the "Enter an address" row
        link.press(Key::A);
        for (const char c : std::string("192.168.10.5")) link.type(c);
        return;
    }
    link.press(Key::A);
    for (int i = 0; i < 4; ++i) link.tick(); // connect, list the games, show them
    if (screen_ == "code") {
        for (const char c : std::string("4829")) link.type(c);
        return;
    }
    if (screen_ == "games") return;
    link.press(Key::Down);
    link.press(Key::Up); // back on the first game
    link.press(Key::A);
    link.tick();
}

} // namespace link_app
