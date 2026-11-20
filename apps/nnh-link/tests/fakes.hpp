#pragma once
// Fakes for LinkState's ports: the test sets what the "server" does next and reads what Link asked.

#include <string>
#include <vector>

#include "ports.hpp"

namespace link_app::test {

struct FakeFinder : Finder {
    std::vector<Server> answers;
    int asks = 0;
    nnh::link::Ipv4 last_also{};
    void ask(nnh::link::Ipv4 also) override {
        ++asks;
        last_also = also;
    }
    bool next(Server& out) override {
        if (answers.empty()) return false;
        out = answers.front();
        answers.erase(answers.begin());
        return true;
    }
};

struct FakeClient : Client {
    // What the test makes the server do.
    bool reachable = true;
    State state = State::Handshake;
    Build build_now = Build::None;
    std::string error_text;
    nnh::link::Id id{};
    std::vector<nnh::link::GameInfo> game_list;
    std::vector<std::string> output;
    std::uint64_t got = 0;
    std::uint64_t size = 0;
    // What Link asked.
    int connects = 0;
    nnh::link::Ipv4 connected_to{};
    std::string code;
    int lists = 0;
    nnh::link::BuildRequest built;
    nnh::link::FileSink* sink = nullptr;
    std::string launched;
    int cancels = 0;

    bool connect(nnh::link::Ipv4 address) override {
        ++connects;
        connected_to = address;
        if (!reachable) error_text = "can't reach 1.2.3.4";
        state = reachable ? State::Handshake : State::Failed;
        return reachable;
    }
    State update() override { return state; }
    bool enter_code(const std::string& c) override {
        code = c;
        state = State::Handshake;
        return true;
    }
    bool list_games() override {
        ++lists;
        state = State::Busy;
        return true;
    }
    bool build(const nnh::link::BuildRequest& request, nnh::link::FileSink& s) override {
        built = request;
        sink = &s;
        state = State::Busy;
        build_now = Build::Building;
        return true;
    }
    bool launching(const std::string& game) override {
        launched = game;
        return true;
    }
    void cancel() override {
        ++cancels;
        state = State::Failed;
        error_text = "cancelled";
    }
    const std::string& error() const override { return error_text; }
    nnh::link::Id server_id() const override { return id; }
    const std::vector<nnh::link::GameInfo>& games() const override { return game_list; }
    Build build_state() const override { return build_now; }
    const std::vector<std::string>& lines() const override { return output; }
    std::uint64_t received() const override { return got; }
    std::uint64_t file_size() const override { return size; }
};

struct NullSink : nnh::link::FileSink {
    bool begin(std::uint64_t) override { return true; }
    bool write(const std::uint8_t*, std::size_t) override { return true; }
    void finish(bool) override {}
};

struct FakeDevice : Device {
    bool can_launch = true;
    NullSink sink;
    std::string sink_name;
    std::string launched;
    nnh::link::Ipv4 log_to{};
    nnh::link::FileSink& sink_for(const std::string& name) override {
        sink_name = name;
        return sink;
    }
    std::string path_of(const std::string& name) override { return "sdmc:/3ds/nnh/" + name + ".3dsx"; }
    bool launch(const std::string& name, nnh::link::Ipv4 server) override {
        if (!can_launch) return false;
        launched = name;
        log_to = server;
        return true;
    }
};

} // namespace link_app::test
