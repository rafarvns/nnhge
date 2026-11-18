#pragma once
// Pairing and authentication (spec 011 FR-3, decision D24), as pure state machines: the sessions feed
// them the messages they receive and send what they return. Both sides start with Hello (id + nonce).
//
//   server knows the console:  S → Proof(server)  C checks it → Proof(console)  S checks it → session
//   first time (no secret):    C → Pair(HMAC(code, …))  S → Paired + Proof(server, new secret) → as above
//
// The server proves itself first, so a console never trusts an impostor. Secrets and keys come from
// HMAC-SHA-256 over both hellos; nothing secret is sent. Unit tested in link/tests/test_handshake.cpp.

#include <optional>
#include <string>
#include <vector>

#include "nnh_link/frame.hpp"
#include "nnh_link/messages.hpp"
#include "nnh_link/sha256.hpp"

namespace nnh::link {

/// A 6-digit pairing code from a random number.
std::string pairing_code(std::uint32_t random);

Digest pair_proof(const std::string& code, const Hello& console, const Hello& server);
Digest derive_secret(const std::string& code, const Hello& console, const Hello& server);
Digest auth_proof(const Digest& secret, Side from, const Hello& console, const Hello& server);
Digest session_key(const Digest& secret, const Hello& console, const Hello& server);

/// Counts wrong pairing codes: after `limit` of them, pairing is refused until restart.
class PairingGuard {
public:
    explicit PairingGuard(int limit = 5) : left_(limit) {}
    bool allowed() const { return left_ > 0; }
    void wrong() {
        if (left_ > 0) --left_;
    }

private:
    int left_;
};

class ServerHandshake {
public:
    explicit ServerHandshake(const Hello& mine) : mine_(mine) {}

    /// The console's Hello. `known` = its secret if it was paired. Returns the frames to send (Hello, and
    /// Proof when known) or a Refused for another protocol version.
    std::vector<std::vector<std::uint8_t>> on_hello(const Hello& console, const std::optional<Digest>& known);
    /// A pairing attempt against the code shown now ("" = none). Returns Paired + Proof, or Refused.
    std::vector<std::vector<std::uint8_t>> on_pair(const Digest& proof, const std::string& code, PairingGuard& guard);
    /// The console's proof. @return true when the console is authenticated (done()).
    bool on_proof(const Digest& proof);

    bool done() const { return done_; }
    /// After a successful pairing: the secret to remember for this console.
    const std::optional<Digest>& new_secret() const { return new_secret_; }
    const Hello& console() const { return console_; }
    Digest key() const { return session_key(*secret_, console_, mine_); }

private:
    Hello mine_;
    Hello console_{};
    std::optional<Digest> secret_;
    std::optional<Digest> new_secret_;
    bool hello_ = false;
    bool done_ = false;
};

class ClientHandshake {
public:
    explicit ClientHandshake(const Hello& mine) : mine_(mine) {}
    std::vector<std::uint8_t> hello() const { return encode_frame(Type::Hello, encode(mine_)); }

    enum class Step { Wait, NeedCode, SendProof, Done, Failed };

    /// The server's Hello; `known` = the secret paired with that server id, if any.
    Step on_hello(const Hello& server, const std::optional<Digest>& known);
    /// The frame that asks to pair with `code` (after NeedCode).
    std::vector<std::uint8_t> pair(const std::string& code);
    /// Paired: the secret is derived from the code given to pair().
    Step on_paired();
    /// The server's proof: checked with the secret; SendProof means send proof() and the session is set.
    Step on_server_proof(const Digest& proof);
    std::vector<std::uint8_t> proof() const;

    const std::string& failure() const { return failure_; }
    /// After pairing: the secret to remember for this server.
    const std::optional<Digest>& new_secret() const { return new_secret_; }
    const Hello& server() const { return server_; }
    Digest key() const { return session_key(*secret_, mine_, server_); }

private:
    Hello mine_;
    Hello server_{};
    std::optional<Digest> secret_;
    std::optional<Digest> new_secret_;
    std::string code_;
    std::string failure_;
    bool hello_ = false;
};

} // namespace nnh::link
