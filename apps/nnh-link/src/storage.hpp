#pragma once
// What NNH Link remembers (spec 011 FR-2, FR-3): this console's id, the servers it paired with (their
// secrets), the last server and the last game. Pure: the bytes are saved by storage_file.cpp with
// nnh::save; unit tested in apps/nnh-link/tests/test_storage.cpp.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "nnh_link/address.hpp"
#include "nnh_link/client_session.hpp"

namespace link_app {

using nnh::link::Digest;
using nnh::link::Id;
using nnh::link::Ipv4;

class Storage : public nnh::link::PairStore {
public:
    static constexpr std::size_t kMaxPairs = 16;
    static constexpr std::uint16_t kFormat = 1;

    // PairStore (used by the session during the handshake).
    std::optional<Digest> secret_of(const Id& server) override;
    void remember(const Id& server, const Digest& secret) override; ///< the oldest goes when full

    bool paired(const Id& server) const;
    void forget(const Id& server);
    std::size_t pair_count() const { return pairs_.size(); }

    const Id& console_id() const { return console_id_; }
    bool has_console_id() const;
    void set_console_id(const Id& id);

    struct LastServer {
        Id id{};
        Ipv4 address;
    };
    const std::optional<LastServer>& last_server() const { return last_server_; }
    void set_last_server(const Id& id, Ipv4 address);
    const std::string& last_game() const { return last_game_; }
    void set_last_game(const std::string& game);

    /// Changed since the last clear_dirty(): time to save.
    bool dirty() const { return dirty_; }
    void clear_dirty() { dirty_ = false; }

    std::vector<std::uint8_t> encode() const;
    /// Replaces everything with `bytes`. @return false (and nothing changed) if they are malformed.
    bool decode(const std::vector<std::uint8_t>& bytes);

private:
    Id console_id_{};
    std::vector<std::pair<Id, Digest>> pairs_; // oldest first
    std::optional<LastServer> last_server_;
    std::string last_game_;
    bool dirty_ = false;
};

} // namespace link_app
