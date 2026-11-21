#include "net_client.hpp"

#include <cstring>

#include "nnh_link/discovery.hpp"

namespace link_app {

using nnh::link::Ipv4;
namespace net = nnh::link::net;

namespace {

constexpr int kConnectTimeoutMs = 3000;
const std::vector<nnh::link::GameInfo> kNoGames;
const std::vector<std::string> kNoLines;

} // namespace

void NetFinder::ask(Ipv4 also) {
    // A fixed port first, like the Homebrew Launcher's netloader: on the console, answers to a socket bound
    // to port 0 never arrived. Any port when it is busy (nnh serve on the same PC as the simulator).
    if (!socket_.valid()) socket_ = net::udp_open(Ipv4{}, nnh::link::kLinkPort);
    if (!socket_.valid()) socket_ = net::udp_open(Ipv4{}, 0);
    if (!socket_.valid()) return;
    const std::size_t size = sizeof nnh::link::kDiscoveryQuestion - 1;
    std::vector<Ipv4> targets = net::local_broadcast_addresses();
    targets.push_back(Ipv4{{255, 255, 255, 255}});
    if (also != Ipv4{}) targets.push_back(also);
    for (const Ipv4& to : targets) net::udp_send(socket_, to, nnh::link::kLinkPort, nnh::link::kDiscoveryQuestion, size);
}

bool NetFinder::next(Server& out) {
    if (!socket_.valid()) return false;
    char buffer[128];
    for (;;) {
        Ipv4 from;
        std::uint16_t port = 0;
        const long n = net::udp_receive(socket_, buffer, sizeof buffer, 0, &from, &port);
        if (n < 0) return false;
        // Only answers from the local network and from nnh serve's port count.
        if (port != nnh::link::kLinkPort || !nnh::link::is_local_network(from)) continue;
        if (nnh::link::parse_discovery_answer(buffer, static_cast<std::size_t>(n), out.id, out.name)) {
            out.address = from;
            return true;
        }
    }
}

bool NetClient::connect(Ipv4 address) {
    session_.reset();
    error_.clear();
    if (!nnh::link::is_local_network(address)) {
        error_ = "not a local network address";
        return false;
    }
    net::Socket socket = net::tcp_connect(address, nnh::link::kLinkPort, kConnectTimeoutMs);
    if (!socket.valid()) {
        error_ = "can't reach " + nnh::link::to_string(address) + ": is nnh serve running there?";
        return false;
    }
    nnh::link::Hello mine;
    mine.id = storage_.console_id();
    random_(mine.nonce.data(), mine.nonce.size());
    session_.emplace(mine, storage_);
    session_->start(static_cast<net::Socket&&>(socket));
    return true;
}

Client::State NetClient::update() { return session_ ? session_->update(0) : State::Failed; }
bool NetClient::enter_code(const std::string& code) { return session_ && session_->enter_code(code); }
bool NetClient::list_games() { return session_ && session_->list_games(); }
bool NetClient::build(const nnh::link::BuildRequest& r, nnh::link::FileSink& s) { return session_ && session_->build(r, s); }
bool NetClient::launching(const std::string& game) { return session_ && session_->launching(game); }
void NetClient::cancel() {
    if (session_) session_->cancel();
}

const std::string& NetClient::error() const { return session_ ? session_->error() : error_; }
nnh::link::Id NetClient::server_id() const { return session_ ? session_->server().id : nnh::link::Id{}; }
const std::vector<nnh::link::GameInfo>& NetClient::games() const { return session_ ? session_->games() : kNoGames; }
Client::Build NetClient::build_state() const { return session_ ? session_->build_state() : Build::None; }
const std::vector<std::string>& NetClient::lines() const { return session_ ? session_->lines() : kNoLines; }
std::uint64_t NetClient::received() const { return session_ ? session_->received() : 0; }
std::uint64_t NetClient::file_size() const { return session_ ? session_->file_size() : 0; }

} // namespace link_app
