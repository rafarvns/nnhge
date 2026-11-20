#pragma once
// NNH_LINK_DEMO=servers|address|code|games|building|receiving|failed|done: NNH Link with a made-up server,
// taken to that screen, for the documentation screenshots (like the scenes example's SCENES_DEMO_START).

#include "link_state.hpp"
#include "nnh_link/client_session.hpp"

namespace link_app {

class DemoPorts final : public Finder, public Client, public Device {
public:
    explicit DemoPorts(const char* screen);

    /// Drives `link` to the screen given to the constructor.
    void advance(LinkState& link);

    // Finder
    void ask(nnh::link::Ipv4) override { asked_ = true; }
    bool next(Server& out) override;
    // Client
    bool connect(nnh::link::Ipv4) override;
    State update() override { return state_; }
    bool enter_code(const std::string&) override { return true; }
    bool list_games() override;
    bool build(const nnh::link::BuildRequest& request, nnh::link::FileSink& sink) override;
    bool launching(const std::string&) override { return true; }
    void cancel() override { state_ = State::Failed; }
    const std::string& error() const override { return error_; }
    nnh::link::Id server_id() const override { return {}; }
    const std::vector<nnh::link::GameInfo>& games() const override { return games_; }
    Build build_state() const override { return build_; }
    const std::vector<std::string>& lines() const override { return lines_; }
    std::uint64_t received() const override { return 412 * 1024; }
    std::uint64_t file_size() const override { return 673 * 1024; }
    // Device
    nnh::link::FileSink& sink_for(const std::string&) override { return sink_; }
    std::string path_of(const std::string& name) override { return "sdmc:/3ds/nnh/" + name + ".3dsx"; }
    bool launch(const std::string&, nnh::link::Ipv4) override { return false; }

private:
    struct NoSink : nnh::link::FileSink {
        bool begin(std::uint64_t) override { return true; }
        bool write(const std::uint8_t*, std::size_t) override { return true; }
        void finish(bool) override {}
    };
    std::string screen_;
    bool asked_ = false;
    int answered_ = 0;
    State state_ = State::Handshake;
    Build build_ = Build::None;
    std::string error_;
    std::vector<nnh::link::GameInfo> games_;
    std::vector<std::string> lines_;
    NoSink sink_;
};

} // namespace link_app
