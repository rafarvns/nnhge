#include "nnh_link/handshake.hpp"

#include <cstdio>

#include "nnh_link/wire.hpp"

namespace nnh::link {

namespace {

/// HMAC(key, label ‖ console id ‖ server id ‖ console nonce ‖ server nonce).
Digest mac_over(const void* key, std::size_t key_size, const char* label, const Hello& console, const Hello& server) {
    Writer w;
    w.str(label);
    w.raw(console.id.data(), console.id.size());
    w.raw(server.id.data(), server.id.size());
    w.raw(console.nonce.data(), console.nonce.size());
    w.raw(server.nonce.data(), server.nonce.size());
    return hmac_sha256(key, key_size, w.bytes().data(), w.bytes().size());
}

bool same(const Digest& a, const Digest& b) { return equal_constant_time(a.data(), b.data(), a.size()); }

} // namespace

std::string pairing_code(std::uint32_t random) {
    char text[8];
    std::snprintf(text, sizeof text, "%06u", static_cast<unsigned>(random % 1000000u));
    return text;
}

Digest pair_proof(const std::string& code, const Hello& console, const Hello& server) {
    return mac_over(code.data(), code.size(), "pair", console, server);
}

Digest derive_secret(const std::string& code, const Hello& console, const Hello& server) {
    return mac_over(code.data(), code.size(), "secret", console, server);
}

Digest auth_proof(const Digest& secret, Side from, const Hello& console, const Hello& server) {
    return mac_over(secret.data(), secret.size(), from == Side::Server ? "server" : "console", console, server);
}

Digest session_key(const Digest& secret, const Hello& console, const Hello& server) {
    return mac_over(secret.data(), secret.size(), "session", console, server);
}

std::vector<std::vector<std::uint8_t>> ServerHandshake::on_hello(const Hello& console, const std::optional<Digest>& known) {
    if (hello_) return {encode_frame(Type::Refused, encode_text("unexpected hello"))};
    hello_ = true;
    console_ = console;
    if (console.version != kProtocolVersion) {
        return {encode_frame(Type::Refused, encode_text("protocol version mismatch: update nnh and NNH Link"))};
    }
    std::vector<std::vector<std::uint8_t>> out = {encode_frame(Type::Hello, encode(mine_))};
    if (known) {
        secret_ = known;
        out.push_back(encode_frame(Type::Proof, encode_digest(auth_proof(*known, Side::Server, console_, mine_))));
    }
    return out;
}

std::vector<std::vector<std::uint8_t>> ServerHandshake::on_pair(const Digest& proof, const std::string& code, PairingGuard& guard) {
    if (!hello_ || done_) return {encode_frame(Type::Refused, encode_text("unexpected pairing"))};
    if (!guard.allowed()) return {encode_frame(Type::Refused, encode_text("too many wrong codes: restart nnh serve"))};
    if (code.empty()) return {encode_frame(Type::Refused, encode_text("no pairing code is shown: start nnh serve"))};
    if (!same(proof, pair_proof(code, console_, mine_))) {
        guard.wrong();
        return {encode_frame(Type::Refused, encode_text("wrong code"))};
    }
    secret_ = new_secret_ = derive_secret(code, console_, mine_);
    return {encode_frame(Type::Paired, {}),
            encode_frame(Type::Proof, encode_digest(auth_proof(*secret_, Side::Server, console_, mine_)))};
}

bool ServerHandshake::on_proof(const Digest& proof) {
    if (!secret_ || done_) return false;
    done_ = same(proof, auth_proof(*secret_, Side::Console, console_, mine_));
    return done_;
}

ClientHandshake::Step ClientHandshake::on_hello(const Hello& server, const std::optional<Digest>& known) {
    if (hello_) {
        failure_ = "unexpected hello";
        return Step::Failed;
    }
    hello_ = true;
    server_ = server;
    if (server.version != kProtocolVersion) {
        failure_ = "protocol version mismatch: update nnh and NNH Link";
        return Step::Failed;
    }
    secret_ = known;
    return known ? Step::Wait : Step::NeedCode; // a known server sends its proof next
}

std::vector<std::uint8_t> ClientHandshake::pair(const std::string& code) {
    code_ = code;
    return encode_frame(Type::Pair, encode_digest(pair_proof(code, mine_, server_)));
}

ClientHandshake::Step ClientHandshake::on_paired() {
    if (!hello_ || code_.empty()) {
        failure_ = "unexpected pairing answer";
        return Step::Failed;
    }
    secret_ = new_secret_ = derive_secret(code_, mine_, server_);
    return Step::Wait;
}

ClientHandshake::Step ClientHandshake::on_server_proof(const Digest& proof) {
    if (!secret_) {
        failure_ = "this PC isn't paired: pair again";
        return Step::Failed;
    }
    if (!same(proof, auth_proof(*secret_, Side::Server, mine_, server_))) {
        failure_ = "the PC failed to prove it is the paired one: pair again";
        new_secret_.reset();
        return Step::Failed;
    }
    return Step::SendProof;
}

std::vector<std::uint8_t> ClientHandshake::proof() const {
    return encode_frame(Type::Proof, encode_digest(auth_proof(*secret_, Side::Console, mine_, server_)));
}

} // namespace nnh::link
