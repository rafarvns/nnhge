#pragma once
// NNH Link's logic (spec 011 FR-2, FR-3, FR-5): which screen is shown and what each key does, over the
// ports (ports.hpp). No drawing and no engine calls: link_scene.cpp turns buttons and touches into
// press()/type() and draws what this exposes. Unit tested against fakes (tests/test_link_state.cpp).
//
//   Servers ─A→ Connecting ─(needs pairing)→ Code ─A→ Connecting ─→ Games ─A→ Building ─→ launch (exit)
//      └A on "Enter an address"→ Address ─A→ Connecting          (PC: Done)   failures → Message

#include <cstdint>
#include <string>
#include <vector>

#include "ports.hpp"
#include "storage.hpp"

namespace link_app {

enum class Screen { Servers, Address, Connecting, Code, Games, Building, Done, Message };
enum class Key { Up, Down, A, B, X, Y, Start };

struct ServerRow {
    Server server;
    bool paired = false;
};

class LinkState {
public:
    static constexpr std::size_t kCodeLength = 6;
    static constexpr std::size_t kMaxAddress = 15;

    LinkState(Finder& finder, Client& client, Device& device, Storage& storage)
        : finder_(finder), client_(client), device_(device), storage_(storage) {}

    /// Asks for servers (the first screen).
    void start();
    /// Every frame, after the keys: answers, the session, the transitions.
    void tick();
    void press(Key key);
    /// A touched row on the servers or games: selects it, then acts like A. Other rows do nothing.
    void choose(int row);
    /// Number pad: '0'-'9' (code, address) and '.' (address).
    void type(char c);
    void erase();

    Screen screen() const { return screen_; }
    /// Set when Link must exit: START on the servers, or the game will start after exit.
    bool quit() const { return quit_; }

    const std::vector<ServerRow>& servers() const { return servers_; }
    /// Servers: 0..servers().size() (the last row is "Enter an address"); Games: the game.
    int selected() const { return selected_; }
    const Server& server() const { return server_; }
    const std::string& typed() const { return typed_; }
    /// Code: the code was sent, waiting for the PC.
    bool checking() const { return checking_; }
    const std::vector<nnh::link::GameInfo>& games() const { return client_.games(); }
    bool release() const { return release_; }
    const nnh::link::GameInfo& game() const { return game_; }
    Client::Build build_state() const { return client_.build_state(); }
    const std::vector<std::string>& lines() const { return lines_; }
    std::uint64_t received() const { return client_.received(); }
    std::uint64_t file_size() const { return client_.file_size(); }
    /// Message: what went wrong; Done: where the game is; Address: why it was refused.
    const std::string& message() const { return message_; }

private:
    void go(Screen screen);
    void show_message(const std::string& text, Screen back, const std::vector<std::string>& lines = {});
    void connect_to(const Server& server);
    void leave_session(); ///< closes the connection, back to the servers
    void tick_servers();
    void tick_session();
    void tick_build();
    void press_servers(Key key);
    void press_games(Key key);
    void submit_address();
    void move(int delta, int count);

    Finder& finder_;
    Client& client_;
    Device& device_;
    Storage& storage_;

    Screen screen_ = Screen::Servers;
    Screen back_ = Screen::Servers; ///< where Message and Done return
    bool quit_ = false;
    std::vector<ServerRow> servers_;
    int selected_ = 0;
    bool user_moved_ = false; ///< the selection follows the last server until the user moves it
    Server server_;
    std::string typed_;
    bool checking_ = false;
    bool connect_pending_ = false; ///< connect on the next tick, after "Connecting" was drawn once
    int frames_on_screen_ = 0;
    bool asked_games_ = false;
    bool release_ = false;
    nnh::link::GameInfo game_;
    std::vector<std::string> lines_;
    std::string message_;
};

} // namespace link_app
