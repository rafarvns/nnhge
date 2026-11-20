#pragma once
// What nnh serve plugs into the link library's server session (spec 011 FR-1, FR-3): the pairings file,
// the pairing code shown in the terminal, the games of the 3DS preset and real builds whose output streams
// to the console. The pairing part is unit tested (tools/nnh/tests/test_serve.cpp); games and builds run
// CMake and are checked by hand.

#include <cstdint>
#include <functional>
#include <string>

#include "nnh_link/server_session.hpp"
#include "serve/pairing_codes.hpp"
#include "serve/pairs_file.hpp"

namespace nnh::tool {

class ServeHooks : public link::ServerHooks {
public:
    using Clock = std::function<std::int64_t()>;     ///< milliseconds
    using Random = std::function<std::uint32_t()>;

    ServeHooks(std::string root, bool release, std::string pairs_path, Clock clock, Random random);

    std::optional<Digest> secret_of(const Id& console) override;
    void remember(const Id& console, const Digest& secret) override;
    std::string pairing_code() override;
    std::vector<link::GameInfo> games() override;
    link::BuildOutput build(const link::BuildRequest& request,
                            const std::function<bool(const std::string&)>& line) override;
    void launching(const std::string& game) override;

    /// Prints a new pairing code (at start-up).
    void show_new_code();
    /// The game the console said it is starting (cleared), "" if none.
    std::string take_launch();

private:
    std::string root_;
    bool release_;
    std::string pairs_path_;
    Clock clock_;
    Random random_;
    PairList pairs_;
    PairingCodes codes_;
    std::string launch_;
};

/// The first 8 hex digits of an id: enough to tell consoles apart in the terminal, nothing secret.
std::string short_id(const Id& id);

} // namespace nnh::tool
