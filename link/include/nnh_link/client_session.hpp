#pragma once
// NNH Link's side of a connection to `nnh serve` (spec 011 FR-2, FR-3, FR-5). It never blocks longer than
// asked: the app calls update(0) every frame and reads the state. The server must prove itself before
// anything it sends is accepted; the received game goes to a FileSink chunk by chunk and is checked
// against its SHA-256. Tested against the real server session on the loopback interface
// (link/tests/test_sessions.cpp).

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nnh_link/connection.hpp"
#include "nnh_link/handshake.hpp"

namespace nnh::link {

/// Remembered pairings, by server id.
class PairStore {
public:
    virtual ~PairStore() = default;
    virtual std::optional<Digest> secret_of(const Id& server) = 0;
    virtual void remember(const Id& server, const Digest& secret) = 0;
};

/// Where the received game goes (the SD card on the console).
class FileSink {
public:
    virtual ~FileSink() = default;
    /// @return false if `size` bytes can't be stored (no space).
    virtual bool begin(std::uint64_t size) = 0;
    virtual bool write(const std::uint8_t* data, std::size_t size) = 0;
    /// The end: `ok` = complete and verified; otherwise drop what was written.
    virtual void finish(bool ok) = 0;
};

struct ClientLimits {
    int handshake_ms = 10000;       ///< waiting for the server during the handshake
    int reply_ms = 15000;           ///< waiting for an answer or the next file chunk
    int build_ms = 10 * 60 * 1000;  ///< silence allowed while the PC builds
};

class ClientSession {
public:
    enum class State { Handshake, NeedCode, Ready, Busy, Failed };
    enum class Build { None, Building, Receiving, Received, Failed };

    ClientSession(const Hello& mine, PairStore& store, const ClientLimits& limits = {})
        : mine_(mine), store_(store), limits_(limits), hs_(mine) {}

    /// Starts on a connected socket (sends Hello).
    void start(net::Socket connected);
    /// In NeedCode: sends the code the user typed.
    bool enter_code(const std::string& code);
    /// In Ready: asks for the game list (state Busy until it arrives).
    bool list_games();
    /// In Ready: asks for a build; the game goes to `sink` (state Busy until it is received or failed).
    bool build(const BuildRequest& request, FileSink& sink);
    /// Tells the server the game is being started (in Ready).
    bool launching(const std::string& game);
    /// B: stops waiting and closes the connection (a partial file is dropped).
    void cancel();

    /// Reads what arrived, waiting up to `wait_ms`, and checks the timeouts.
    State update(int wait_ms);

    State state() const { return state_; }
    const std::string& error() const { return error_; }
    const Hello& server() const { return hs_.server(); }
    bool paired_now() const { return paired_now_; }
    const std::vector<GameInfo>& games() const { return games_; }
    Build build_state() const { return build_; }
    /// The build's last output lines (the error lines after a failed build).
    const std::vector<std::string>& lines() const { return lines_; }
    std::uint64_t received() const { return received_; }
    std::uint64_t file_size() const { return file_size_; }

private:
    void fail(const std::string& why);
    void on_handshake(const Frame& f);
    void on_message(const Frame& f);
    void on_chunk(const std::vector<std::uint8_t>& data);
    int timeout_ms() const;

    Hello mine_;
    PairStore& store_;
    ClientLimits limits_;
    ClientHandshake hs_;
    Connection connection_;
    State state_ = State::Handshake;
    std::string error_;
    bool got_hello_ = false;
    bool code_sent_ = false;
    bool paired_ = false;
    bool paired_now_ = false;
    std::vector<GameInfo> games_;
    Build build_ = Build::None;
    std::vector<std::string> lines_;
    FileSink* sink_ = nullptr;
    Sha256 hash_;
    Digest expected_{};
    std::uint64_t file_size_ = 0;
    std::uint64_t received_ = 0;
    std::int64_t last_activity_ms_ = 0;
};

} // namespace nnh::link
