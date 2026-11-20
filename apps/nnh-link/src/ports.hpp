#pragma once
// What LinkState talks to (spec 011): finding servers, a session with one, and the device (where the
// game goes, launching it). The app plugs the real ones in (net_client.cpp, device.cpp); the tests and the
// screenshot demo plug fakes in.

#include <cstdint>
#include <string>
#include <vector>

#include "nnh_link/address.hpp"
#include "nnh_link/client_session.hpp"
#include "nnh_link/messages.hpp"

namespace link_app {

struct Server {
    nnh::link::Ipv4 address;
    nnh::link::Id id{}; ///< all zero when typed by hand (known after the handshake)
    std::string name;
};

class Finder {
public:
    virtual ~Finder() = default;
    /// Asks the local network (and `also`, when it isn't 0.0.0.0) which servers there are.
    virtual void ask(nnh::link::Ipv4 also) = 0;
    /// One answer that arrived, without waiting. @return false when there is none.
    virtual bool next(Server& out) = 0;
};

class Client {
public:
    using State = nnh::link::ClientSession::State;
    using Build = nnh::link::ClientSession::Build;

    virtual ~Client() = default;
    /// Connects (may take a few seconds) and starts a new session. @return false if unreachable (error()).
    virtual bool connect(nnh::link::Ipv4 address) = 0;
    /// Reads what arrived, without waiting.
    virtual State update() = 0;
    virtual bool enter_code(const std::string& code) = 0;
    virtual bool list_games() = 0;
    virtual bool build(const nnh::link::BuildRequest& request, nnh::link::FileSink& sink) = 0;
    virtual bool launching(const std::string& game) = 0;
    virtual void cancel() = 0;

    virtual const std::string& error() const = 0;
    virtual nnh::link::Id server_id() const = 0;
    virtual const std::vector<nnh::link::GameInfo>& games() const = 0;
    virtual Build build_state() const = 0;
    virtual const std::vector<std::string>& lines() const = 0;
    virtual std::uint64_t received() const = 0;
    virtual std::uint64_t file_size() const = 0;
};

class Device {
public:
    virtual ~Device() = default;
    /// Where the game `name` is written as it arrives.
    virtual nnh::link::FileSink& sink_for(const std::string& name) = 0;
    /// The received game's location, to show ("sdmc:/3ds/nnh/audio.3dsx").
    virtual std::string path_of(const std::string& name) = 0;
    /// Arranges for the received game to start when Link exits, sending its log to `server`.
    /// @return false where that isn't possible (the PC simulator, a .cia).
    virtual bool launch(const std::string& name, nnh::link::Ipv4 server) = 0;
};

} // namespace link_app
