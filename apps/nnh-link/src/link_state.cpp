#include "link_state.hpp"

namespace link_app {

using nnh::link::Ipv4;

void LinkState::start() {
    servers_.clear();
    selected_ = 0;
    user_moved_ = false;
    frames_on_screen_ = 0;
    // The last PC is listed by its address until it answers: if the search fails (broadcasts blocked,
    // answers lost), it can still be picked without typing the address again.
    if (const auto& last = storage_.last_server()) {
        servers_.push_back({Server{last->address, last->id, nnh::link::to_string(last->address)}, false});
    }
    ask();
}

void LinkState::ask() {
    Ipv4 also{};
    if (storage_.last_server()) also = storage_.last_server()->address;
    finder_.ask(also);
}

void LinkState::go(Screen screen) {
    screen_ = screen;
    frames_on_screen_ = 0;
}

void LinkState::show_message(const std::string& text, Screen back, const std::vector<std::string>& lines) {
    message_ = text;
    lines_ = lines;
    back_ = back;
    go(Screen::Message);
}

void LinkState::connect_to(const Server& server) {
    server_ = server;
    asked_games_ = false;
    checking_ = false;
    typed_.clear();
    connect_pending_ = true;
    go(Screen::Connecting);
}

void LinkState::leave_session() {
    client_.cancel();
    connect_pending_ = false;
    start();
    go(Screen::Servers);
}

void LinkState::move(int delta, int count) {
    if (count <= 0) {
        selected_ = 0;
        return;
    }
    selected_ = ((selected_ + delta) % count + count) % count;
}

void LinkState::tick() {
    switch (screen_) {
    case Screen::Servers: tick_servers(); break;
    case Screen::Connecting:
    case Screen::Code:
    case Screen::Games: tick_session(); break;
    case Screen::Building: tick_build(); break;
    default: break;
    }
    ++frames_on_screen_;
}

void LinkState::tick_servers() {
    if (frames_on_screen_ > 0 && frames_on_screen_ % kSearchEveryFrames == 0) ask();
    Server found;
    while (finder_.next(found)) {
        bool known = false;
        for (ServerRow& row : servers_) {
            if (row.server.id == found.id) {
                row.server = found; // it may have moved to another address
                known = true;
            }
        }
        if (!known) servers_.push_back({found, false});
    }
    const auto& last = storage_.last_server();
    for (std::size_t i = 0; i < servers_.size(); ++i) {
        servers_[i].paired = storage_.paired(servers_[i].server.id);
        if (!user_moved_ && last && last->id == servers_[i].server.id) selected_ = static_cast<int>(i);
    }
}

void LinkState::tick_session() {
    if (connect_pending_) {
        if (frames_on_screen_ < 1) return; // let "Connecting" show first: connecting can take seconds
        connect_pending_ = false;
        if (!client_.connect(server_.address)) show_message(client_.error(), Screen::Servers);
        return;
    }
    const Client::State state = client_.update();
    if (state == Client::State::Failed) {
        show_message(client_.error(), Screen::Servers);
    } else if (state == Client::State::NeedCode && screen_ != Screen::Code) {
        typed_.clear();
        checking_ = false;
        go(Screen::Code);
    } else if (state == Client::State::Ready && !asked_games_) {
        asked_games_ = true;
        client_.list_games();
        go(Screen::Connecting);
    } else if (state == Client::State::Ready && screen_ != Screen::Games) {
        server_.id = client_.server_id();
        storage_.set_last_server(server_.id, server_.address);
        selected_ = 0;
        const auto& games = client_.games();
        for (std::size_t i = 0; i < games.size(); ++i) {
            if (games[i].name == storage_.last_game()) selected_ = static_cast<int>(i);
        }
        go(Screen::Games);
    }
}

void LinkState::tick_build() {
    const Client::State state = client_.update();
    lines_ = client_.lines();
    if (state == Client::State::Failed) {
        show_message(client_.error(), Screen::Servers, lines_);
    } else if (client_.build_state() == Client::Build::Failed) {
        show_message("the build failed", Screen::Games, lines_);
    } else if (client_.build_state() == Client::Build::Received) {
        if (device_.launch(game_.name, server_.address)) {
            client_.launching(game_.name); // nnh serve then shows the game's log
            quit_ = true;
        } else {
            message_ = device_.path_of(game_.name);
            back_ = Screen::Games;
            go(Screen::Done);
        }
    }
}

void LinkState::press(Key key) {
    switch (screen_) {
    case Screen::Servers: press_servers(key); break;
    case Screen::Address:
        if (key == Key::A) submit_address();
        if (key == Key::B) go(Screen::Servers);
        break;
    case Screen::Code:
        if (key == Key::A && !checking_ && typed_.size() == kCodeLength) {
            checking_ = true;
            client_.enter_code(typed_);
        }
        if (key == Key::B) leave_session();
        break;
    case Screen::Connecting:
        if (key == Key::B) leave_session();
        break;
    case Screen::Games: press_games(key); break;
    case Screen::Building:
        if (key == Key::B) {
            client_.cancel();
            start();
            show_message("build cancelled", Screen::Servers);
        }
        break;
    case Screen::Done:
    case Screen::Message:
        if (key == Key::A || key == Key::B) {
            if (back_ == Screen::Servers) start();
            go(back_);
        }
        break;
    }
}

void LinkState::choose(int row) {
    int rows = 0;
    if (screen_ == Screen::Servers) rows = static_cast<int>(servers_.size()) + 1;
    if (screen_ == Screen::Games) rows = static_cast<int>(client_.games().size());
    if (row < 0 || row >= rows) return;
    selected_ = row;
    user_moved_ = true;
    press(Key::A);
}

void LinkState::press_servers(Key key) {
    const int rows = static_cast<int>(servers_.size()) + 1; // + "Enter an address"
    if (key == Key::Up || key == Key::Down) {
        move(key == Key::Up ? -1 : 1, rows);
        user_moved_ = true;
    } else if (key == Key::A && selected_ < static_cast<int>(servers_.size())) {
        connect_to(servers_[static_cast<std::size_t>(selected_)].server);
    } else if (key == Key::A) {
        typed_.clear();
        message_.clear();
        go(Screen::Address);
    } else if (key == Key::X) {
        start();
    } else if (key == Key::Y && selected_ < static_cast<int>(servers_.size())) {
        ServerRow& row = servers_[static_cast<std::size_t>(selected_)];
        storage_.forget(row.server.id);
        row.paired = false;
    } else if (key == Key::Start) {
        quit_ = true;
    }
}

void LinkState::press_games(Key key) {
    const auto& games = client_.games();
    if (key == Key::Up || key == Key::Down) {
        move(key == Key::Up ? -1 : 1, static_cast<int>(games.size()));
    } else if (key == Key::A && !games.empty()) {
        game_ = games[static_cast<std::size_t>(selected_)];
        lines_.clear();
        storage_.set_last_game(game_.name);
        client_.build({game_.name, release_}, device_.sink_for(game_.name));
        go(Screen::Building);
    } else if (key == Key::X) {
        release_ = !release_;
    } else if (key == Key::B) {
        leave_session();
    }
}

void LinkState::submit_address() {
    const auto address = nnh::link::parse_ipv4(typed_);
    if (!address || !nnh::link::is_local_network(*address)) {
        message_ = "enter a local network address, like 192.168.0.10";
        return;
    }
    connect_to(Server{*address, {}, typed_});
}

void LinkState::type(char c) {
    const bool digit = c >= '0' && c <= '9';
    if (screen_ == Screen::Code && digit && !checking_ && typed_.size() < kCodeLength) typed_ += c;
    if (screen_ == Screen::Address && (digit || c == '.') && typed_.size() < kMaxAddress) typed_ += c;
}

void LinkState::erase() {
    if ((screen_ == Screen::Code && !checking_) || screen_ == Screen::Address) {
        if (!typed_.empty()) typed_.pop_back();
    }
}

} // namespace link_app
