#pragma once
// `nnh serve`'s side of one console connection (spec 011 FR-1, FR-3, FR-5): handshake (pairing or
// authentication), then list, build, send the file and hear about the launch, until the console leaves.
// Everything outside the protocol — the pairing store, the code shown, the games, the build itself — comes
// through ServerHooks, so the session is tested against a fake (link/tests/test_sessions.cpp).

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "nnh_link/handshake.hpp"
#include "nnh_link/net.hpp"

namespace nnh::link {

struct BuildOutput {
    bool ok = false;
    std::string file;               ///< the built .3dsx when ok
    std::vector<std::string> lines; ///< the output's last lines when not ok
};

class ServerHooks {
public:
    virtual ~ServerHooks() = default;
    virtual std::optional<Digest> secret_of(const Id& console) = 0;
    virtual void remember(const Id& console, const Digest& secret) = 0;
    /// The pairing code shown right now, "" if none.
    virtual std::string pairing_code() = 0;
    virtual std::vector<GameInfo> games() = 0;
    /// Builds a game. `line` receives each output line and returns false once the console has gone (cancel):
    /// stop building then.
    virtual BuildOutput build(const BuildRequest& request, const std::function<bool(const std::string&)>& line) = 0;
    /// The console is starting `game` (its log comes on the netloader's log port).
    virtual void launching(const std::string& game) = 0;
};

struct ServeLimits {
    int handshake_ms = 2 * 60 * 1000; ///< each handshake step (long enough to type the code)
    int idle_ms = 30 * 60 * 1000;     ///< between requests
};

/// Serves one connected console until it leaves or breaks the protocol. @return why it ended, for the
/// terminal ("console left", "wrong code", ...). Never logs a secret.
std::string serve_console(net::Socket socket, const Hello& mine, ServerHooks& hooks, PairingGuard& guard,
                          const ServeLimits& limits = {});

} // namespace nnh::link
