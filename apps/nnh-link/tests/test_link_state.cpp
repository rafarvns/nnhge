// NNH Link's screens and keys (apps/nnh-link/src/link_state.cpp, spec 011 FR-2, FR-3, FR-5), against fakes.

#include "fakes.hpp"
#include "link_state.hpp"
#include "test.hpp"

using namespace link_app;
using link_app::test::FakeClient;
using link_app::test::FakeDevice;
using link_app::test::FakeFinder;
using nnh::link::Ipv4;
using State = Client::State;
using Build = Client::Build;

namespace {

nnh::link::Id id_of(std::uint8_t n) {
    nnh::link::Id id{};
    id[0] = n;
    return id;
}

Server server(std::uint8_t n, const char* name) { return Server{Ipv4{{192, 168, 0, n}}, id_of(n), name}; }

struct Fixture {
    FakeFinder finder;
    FakeClient client;
    FakeDevice device;
    Storage storage;
    LinkState link{finder, client, device, storage};

    Fixture() {
        client.id = id_of(7);
        client.game_list = {{"hello", "Hello NNHGE"}, {"audio", "NNHGE Audio"}};
    }
    void ticks(int n) {
        for (int i = 0; i < n; ++i) link.tick();
    }
    /// From the servers screen to the games of server 7 (already paired).
    void reach_games() {
        finder.answers = {server(7, "RAFA")};
        link.start();
        link.tick();
        link.press(Key::A);
        ticks(2);                   // "Connecting" drawn, then connect
        client.state = State::Ready; // handshake done
        link.tick();                // asks for the games
        client.state = State::Ready; // games arrived
        link.tick();
    }
    void type(const char* text) {
        for (const char* c = text; *c != '\0'; ++c) link.type(*c);
    }
};

} // namespace

NNH_TEST(link_lists_the_servers_that_answer_and_marks_the_paired_ones) {
    Fixture f;
    f.storage.remember(id_of(2), {});
    f.finder.answers = {server(1, "A"), server(2, "B")};
    f.link.start();
    f.link.tick();
    CHECK(f.finder.asks == 1);
    CHECK(f.link.screen() == Screen::Servers);
    CHECK(f.link.servers().size() == 2);
    CHECK(!f.link.servers()[0].paired);
    CHECK(f.link.servers()[1].paired);
}

NNH_TEST(link_keeps_one_row_per_server_when_it_answers_again) {
    Fixture f;
    f.finder.answers = {server(1, "A"), server(1, "A"), Server{Ipv4{{10, 0, 0, 9}}, id_of(1), "A2"}};
    f.link.start();
    f.link.tick();
    CHECK(f.link.servers().size() == 1);
    CHECK(f.link.servers()[0].server.name == "A2");
    CHECK(f.link.servers()[0].server.address == (Ipv4{{10, 0, 0, 9}}));
}

NNH_TEST(link_asks_the_last_server_directly_and_selects_it) {
    Fixture f;
    f.storage.set_last_server(id_of(2), Ipv4{{192, 168, 0, 2}});
    f.finder.answers = {server(1, "A"), server(2, "B")};
    f.link.start();
    f.link.tick();
    CHECK(f.finder.last_also == (Ipv4{{192, 168, 0, 2}}));
    CHECK(f.link.selected() == 1);
}

NNH_TEST(link_selection_moves_and_wraps_over_the_address_row) {
    Fixture f;
    f.finder.answers = {server(1, "A")};
    f.link.start();
    f.link.tick();
    f.link.press(Key::Down);
    CHECK(f.link.selected() == 1); // "Enter an address"
    f.link.press(Key::Down);
    CHECK(f.link.selected() == 0);
    f.link.press(Key::Up);
    CHECK(f.link.selected() == 1);
}

NNH_TEST(link_shows_connecting_before_it_connects) {
    Fixture f;
    f.finder.answers = {server(7, "RAFA")};
    f.link.start();
    f.link.tick();
    f.link.press(Key::A);
    CHECK(f.link.screen() == Screen::Connecting);
    f.link.tick(); // the frame that draws "Connecting"
    CHECK(f.client.connects == 0);
    f.link.tick();
    CHECK(f.client.connects == 1);
    CHECK(f.client.connected_to == (Ipv4{{192, 168, 0, 7}}));
}

NNH_TEST(link_reports_an_unreachable_server_and_goes_back) {
    Fixture f;
    f.client.reachable = false;
    f.finder.answers = {server(7, "RAFA")};
    f.link.start();
    f.link.tick();
    f.link.press(Key::A);
    f.ticks(2);
    CHECK(f.link.screen() == Screen::Message);
    CHECK(f.link.message() == "can't reach 1.2.3.4");
    f.link.press(Key::A);
    CHECK(f.link.screen() == Screen::Servers);
    CHECK(f.finder.asks == 2); // searched again
}

NNH_TEST(link_reaches_the_games_and_remembers_the_server) {
    Fixture f;
    f.reach_games();
    CHECK(f.client.lists == 1);
    CHECK(f.link.screen() == Screen::Games);
    CHECK(f.link.games().size() == 2);
    CHECK(f.storage.last_server().has_value());
    CHECK(f.storage.last_server()->id == id_of(7));
}

NNH_TEST(link_selects_the_last_game) {
    Fixture f;
    f.storage.set_last_game("audio");
    f.reach_games();
    CHECK(f.link.selected() == 1);
}

NNH_TEST(link_asks_the_code_types_it_and_sends_six_digits) {
    Fixture f;
    f.finder.answers = {server(7, "RAFA")};
    f.link.start();
    f.link.tick();
    f.link.press(Key::A);
    f.ticks(2);
    f.client.state = State::NeedCode;
    f.link.tick();
    CHECK(f.link.screen() == Screen::Code);
    f.type("12a345");
    CHECK(f.link.typed() == "12345");
    f.link.press(Key::A); // 5 digits: not yet
    CHECK(f.client.code.empty());
    f.type("67");
    CHECK(f.link.typed() == "123456");
    f.link.erase();
    f.link.type('9');
    f.link.press(Key::A);
    CHECK(f.client.code == "123459");
    CHECK(f.link.checking());
    f.link.type('1'); // locked while checking
    f.link.erase();
    CHECK(f.link.typed() == "123459");
    f.client.state = State::Ready; // paired
    f.link.tick();
    f.client.state = State::Ready;
    f.link.tick();
    CHECK(f.link.screen() == Screen::Games);
}

NNH_TEST(link_reports_a_refused_code) {
    Fixture f;
    f.finder.answers = {server(7, "RAFA")};
    f.link.start();
    f.link.tick();
    f.link.press(Key::A);
    f.ticks(2);
    f.client.state = State::NeedCode;
    f.link.tick();
    f.type("000000");
    f.link.press(Key::A);
    f.client.state = State::Failed;
    f.client.error_text = "the PC refused: wrong pairing code";
    f.link.tick();
    CHECK(f.link.screen() == Screen::Message);
    CHECK(f.link.message() == "the PC refused: wrong pairing code");
}

NNH_TEST(link_b_leaves_the_code_screen_and_closes_the_session) {
    Fixture f;
    f.finder.answers = {server(7, "RAFA")};
    f.link.start();
    f.link.tick();
    f.link.press(Key::A);
    f.ticks(2);
    f.client.state = State::NeedCode;
    f.link.tick();
    f.link.press(Key::B);
    CHECK(f.client.cancels == 1);
    CHECK(f.link.screen() == Screen::Servers);
}

NNH_TEST(link_builds_the_selected_game_development_or_release) {
    Fixture f;
    f.reach_games();
    f.link.press(Key::Down);
    f.link.press(Key::X);
    CHECK(f.link.release());
    f.link.press(Key::A);
    CHECK(f.link.screen() == Screen::Building);
    CHECK(f.client.built.game == "audio");
    CHECK(f.client.built.release);
    CHECK(f.device.sink_name == "audio");
    CHECK(f.storage.last_game() == "audio");
}

NNH_TEST(link_launches_the_received_game_and_quits) {
    Fixture f;
    f.reach_games();
    f.link.press(Key::A);
    f.client.output = {"[1/2] Building hello.cpp"};
    f.link.tick();
    CHECK(f.link.lines().size() == 1);
    f.client.build_now = Build::Received;
    f.client.state = State::Ready;
    f.link.tick();
    CHECK(f.device.launched == "hello");
    CHECK(f.device.log_to == (Ipv4{{192, 168, 0, 7}}));
    CHECK(f.client.launched == "hello");
    CHECK(f.link.quit());
}

NNH_TEST(link_without_launching_shows_where_the_game_is) {
    Fixture f;
    f.device.can_launch = false;
    f.reach_games();
    f.link.press(Key::A);
    f.client.build_now = Build::Received;
    f.client.state = State::Ready;
    f.link.tick();
    CHECK(f.link.screen() == Screen::Done);
    CHECK(f.link.message() == "sdmc:/3ds/nnh/hello.3dsx");
    CHECK(f.client.launched.empty()); // nnh serve isn't told: no log will come
    CHECK(!f.link.quit());
    f.link.press(Key::A);
    CHECK(f.link.screen() == Screen::Games);
}

NNH_TEST(link_shows_the_error_lines_of_a_failed_build) {
    Fixture f;
    f.reach_games();
    f.link.press(Key::A);
    f.client.output = {"main.cpp:3: error: x", "ninja: build stopped"};
    f.client.build_now = Build::Failed;
    f.client.state = State::Ready;
    f.link.tick();
    CHECK(f.link.screen() == Screen::Message);
    CHECK(f.link.message() == "the build failed");
    CHECK(f.link.lines().size() == 2);
    f.link.press(Key::B);
    CHECK(f.link.screen() == Screen::Games); // the session is still open
}

NNH_TEST(link_reports_a_server_gone_during_the_build) {
    Fixture f;
    f.reach_games();
    f.link.press(Key::A);
    f.client.state = State::Failed;
    f.client.error_text = "the PC went away during the transfer";
    f.link.tick();
    CHECK(f.link.screen() == Screen::Message);
    CHECK(f.link.message() == "the PC went away during the transfer");
    f.link.press(Key::A);
    CHECK(f.link.screen() == Screen::Servers);
}

NNH_TEST(link_b_cancels_the_build) {
    Fixture f;
    f.reach_games();
    f.link.press(Key::A);
    f.link.press(Key::B);
    CHECK(f.client.cancels == 1);
    CHECK(f.link.screen() == Screen::Message);
    CHECK(f.link.message() == "build cancelled");
}

NNH_TEST(link_b_on_the_games_leaves_the_server) {
    Fixture f;
    f.reach_games();
    f.link.press(Key::B);
    CHECK(f.client.cancels == 1);
    CHECK(f.link.screen() == Screen::Servers);
}

NNH_TEST(link_reports_a_server_gone_while_on_the_games) {
    Fixture f;
    f.reach_games();
    f.client.state = State::Failed;
    f.client.error_text = "the PC closed the connection";
    f.link.tick();
    CHECK(f.link.screen() == Screen::Message);
}

NNH_TEST(link_connects_to_a_typed_local_address) {
    Fixture f;
    f.link.start();
    f.link.tick();
    f.link.press(Key::A); // no servers: the only row is "Enter an address"
    CHECK(f.link.screen() == Screen::Address);
    f.type("8.8.8.8");
    f.link.press(Key::A);
    CHECK(f.link.screen() == Screen::Address);
    CHECK(!f.link.message().empty());
    for (int i = 0; i < 7; ++i) f.link.erase();
    f.type("10.0.0.5x");
    CHECK(f.link.typed() == "10.0.0.5");
    f.link.press(Key::A);
    CHECK(f.link.screen() == Screen::Connecting);
    f.ticks(2);
    CHECK(f.client.connected_to == (Ipv4{{10, 0, 0, 5}}));
}

NNH_TEST(link_y_forgets_the_selected_pairing) {
    Fixture f;
    f.storage.remember(id_of(1), {});
    f.finder.answers = {server(1, "A")};
    f.link.start();
    f.link.tick();
    f.link.press(Key::Y);
    CHECK(!f.storage.paired(id_of(1)));
    CHECK(!f.link.servers()[0].paired);
}

NNH_TEST(link_touching_a_row_chooses_it) {
    Fixture f;
    f.finder.answers = {server(1, "A"), server(7, "B")};
    f.link.start();
    f.link.tick();
    f.link.choose(5); // no such row
    CHECK(f.link.screen() == Screen::Servers);
    f.link.choose(1);
    CHECK(f.link.screen() == Screen::Connecting);
    CHECK(f.link.server().name == "B");
}

NNH_TEST(link_touching_a_game_builds_it) {
    Fixture f;
    f.reach_games();
    f.link.choose(2); // only two games
    CHECK(f.link.screen() == Screen::Games);
    f.link.choose(1);
    CHECK(f.link.screen() == Screen::Building);
    CHECK(f.client.built.game == "audio");
}

NNH_TEST(link_start_on_the_servers_quits) {
    Fixture f;
    f.link.start();
    f.link.press(Key::Start);
    CHECK(f.link.quit());
}
