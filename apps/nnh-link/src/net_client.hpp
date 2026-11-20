#pragma once
// The real Finder and Client (ports.hpp) over the nnh_link library: UDP discovery and a ClientSession.
// Checked by the loopback session tests of link/ and by hand against nnh serve.

#include <optional>

#include "nnh_link/net.hpp"
#include "ports.hpp"
#include "storage.hpp"

namespace link_app {

class NetFinder : public Finder {
public:
    void ask(nnh::link::Ipv4 also) override;
    bool next(Server& out) override;

private:
    nnh::link::net::Socket socket_;
};

class NetClient : public Client {
public:
    /// `random` fills the nonce of each connection.
    NetClient(Storage& storage, void (*random)(std::uint8_t* out, std::size_t size))
        : storage_(storage), random_(random) {}

    bool connect(nnh::link::Ipv4 address) override;
    State update() override;
    bool enter_code(const std::string& code) override;
    bool list_games() override;
    bool build(const nnh::link::BuildRequest& request, nnh::link::FileSink& sink) override;
    bool launching(const std::string& game) override;
    void cancel() override;

    const std::string& error() const override;
    nnh::link::Id server_id() const override;
    const std::vector<nnh::link::GameInfo>& games() const override;
    Build build_state() const override;
    const std::vector<std::string>& lines() const override;
    std::uint64_t received() const override;
    std::uint64_t file_size() const override;

private:
    Storage& storage_;
    void (*random_)(std::uint8_t*, std::size_t);
    std::optional<nnh::link::ClientSession> session_;
    std::string error_; ///< when there is no session (connect failed)
};

} // namespace link_app
