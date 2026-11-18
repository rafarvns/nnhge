// NNH Link pairing and authentication (handshake.hpp, spec 011 FR-3): first pairing, reconnection with
// the remembered secret, wrong codes and the rate limit, an impostor server, an unpaired console, and
// replayed or tampered proofs.

#include <optional>
#include <string>
#include <vector>

#include "nnh_link/handshake.hpp"
#include "test.hpp"

using namespace nnh::link;

namespace {

Hello hello_of(std::uint8_t id, std::uint8_t nonce) {
    Hello h;
    h.id.fill(id);
    h.nonce.fill(nonce);
    return h;
}

Frame frame_of(const std::vector<std::uint8_t>& bytes) {
    FrameReader r;
    r.feed(bytes.data(), bytes.size());
    bool error = false;
    auto f = r.next(error);
    return f ? *f : Frame{};
}

Digest digest_in(const std::vector<std::uint8_t>& bytes) {
    Digest d{};
    decode_digest(frame_of(bytes).payload, d);
    return d;
}

std::vector<Type> types_of(const std::vector<std::vector<std::uint8_t>>& frames) {
    std::vector<Type> types;
    for (const auto& f : frames) types.push_back(frame_of(f).type);
    return types;
}

/// Runs a whole pairing with `code` typed on the console. @return the secret both sides remember.
std::optional<Digest> pair_once(const Hello& console_hello, const Hello& server_hello, const std::string& code) {
    ServerHandshake server(server_hello);
    ClientHandshake console(console_hello);
    server.on_hello(console_hello, std::nullopt);
    if (console.on_hello(server_hello, std::nullopt) != ClientHandshake::Step::NeedCode) return std::nullopt;
    PairingGuard guard;
    const auto answer = server.on_pair(digest_in(console.pair(code)), code, guard);
    if (types_of(answer) != std::vector<Type>{Type::Paired, Type::Proof}) return std::nullopt;
    console.on_paired();
    if (console.on_server_proof(digest_in(answer[1])) != ClientHandshake::Step::SendProof) return std::nullopt;
    if (!server.on_proof(digest_in(console.proof()))) return std::nullopt;
    if (!server.new_secret() || server.new_secret() != console.new_secret()) return std::nullopt;
    if (server.key() != console.key()) return std::nullopt;
    return server.new_secret();
}

} // namespace

NNH_TEST(pairing_codes_have_six_digits) {
    CHECK(pairing_code(0) == "000000" && pairing_code(42) == "000042" && pairing_code(4294967295u) == "967295");
}

NNH_TEST(pairing_with_the_right_code_gives_both_sides_the_same_secret_and_key) {
    CHECK(pair_once(hello_of(1, 2), hello_of(3, 4), "123456").has_value());
}

NNH_TEST(a_paired_console_reconnects_with_fresh_keys_and_no_code) {
    const Hello first_console = hello_of(1, 2), first_server = hello_of(3, 4);
    const auto secret = pair_once(first_console, first_server, "123456");
    CHECK(secret.has_value());
    Digest old_key = session_key(*secret, first_console, first_server);

    const Hello console_hello = hello_of(1, 5), server_hello = hello_of(3, 6); // new nonces
    ServerHandshake server(server_hello);
    ClientHandshake console(console_hello);
    const auto out = server.on_hello(console_hello, secret);
    CHECK(types_of(out) == (std::vector<Type>{Type::Hello, Type::Proof}));
    CHECK(console.on_hello(server_hello, secret) == ClientHandshake::Step::Wait);
    CHECK(console.on_server_proof(digest_in(out[1])) == ClientHandshake::Step::SendProof);
    CHECK(server.on_proof(digest_in(console.proof())) && server.done());
    CHECK(!server.new_secret() && !console.new_secret());
    CHECK(server.key() == console.key() && server.key() != old_key);
}

NNH_TEST(wrong_codes_are_refused_and_five_lock_pairing) {
    const Hello console_hello = hello_of(1, 2), server_hello = hello_of(3, 4);
    PairingGuard guard;
    for (int i = 0; i < 5; ++i) {
        ServerHandshake server(server_hello);
        ClientHandshake console(console_hello);
        server.on_hello(console_hello, std::nullopt);
        console.on_hello(server_hello, std::nullopt);
        const auto out = server.on_pair(digest_in(console.pair("000000")), "123456", guard);
        CHECK(types_of(out) == std::vector<Type>{Type::Refused});
        CHECK(!server.new_secret() && !server.on_proof(Digest{}));
    }
    ServerHandshake server(server_hello);
    ClientHandshake console(console_hello);
    server.on_hello(console_hello, std::nullopt);
    console.on_hello(server_hello, std::nullopt);
    const auto out = server.on_pair(digest_in(console.pair("123456")), "123456", guard); // right, but locked
    std::string why;
    CHECK(types_of(out) == std::vector<Type>{Type::Refused} && decode_text(frame_of(out[0]).payload, why));
    CHECK(why.find("too many") != std::string::npos);
}

NNH_TEST(pairing_is_refused_when_no_code_is_shown) {
    ServerHandshake server(hello_of(3, 4));
    ClientHandshake console(hello_of(1, 2));
    server.on_hello(hello_of(1, 2), std::nullopt);
    console.on_hello(hello_of(3, 4), std::nullopt);
    PairingGuard guard;
    CHECK(types_of(server.on_pair(digest_in(console.pair("")), "", guard)) == std::vector<Type>{Type::Refused});
    CHECK(guard.allowed());
}

NNH_TEST(an_impostor_server_is_caught_before_the_console_proves_itself) {
    const Hello console_hello = hello_of(1, 2), real = hello_of(3, 4);
    const auto secret = pair_once(console_hello, real, "123456");
    // The impostor copies the real server's id but doesn't know the secret.
    Digest guess;
    guess.fill(0xAB);
    ServerHandshake impostor(hello_of(3, 9));
    ClientHandshake console(console_hello);
    const auto out = impostor.on_hello(console_hello, guess);
    CHECK(console.on_hello(hello_of(3, 9), secret) == ClientHandshake::Step::Wait);
    CHECK(console.on_server_proof(digest_in(out[1])) == ClientHandshake::Step::Failed);
    CHECK(console.failure().find("prove") != std::string::npos);
}

NNH_TEST(an_unpaired_console_cannot_pass_the_proof) {
    const Hello console_hello = hello_of(1, 2), server_hello = hello_of(3, 4);
    const auto secret = pair_once(hello_of(7, 7), server_hello, "123456"); // another console's secret
    ServerHandshake server(server_hello);
    server.on_hello(console_hello, std::nullopt);
    CHECK(!server.on_proof(auth_proof(*secret, Side::Console, console_hello, server_hello)) && !server.done());
    // A console told it is known, without the secret, fails the server's proof.
    ClientHandshake stranger(console_hello);
    CHECK(stranger.on_hello(server_hello, std::nullopt) == ClientHandshake::Step::NeedCode);
    CHECK(stranger.on_server_proof(Digest{}) == ClientHandshake::Step::Failed);
}

NNH_TEST(replayed_or_tampered_proofs_are_refused) {
    const Hello console_hello = hello_of(1, 2), server_hello = hello_of(3, 4);
    const auto secret = pair_once(console_hello, server_hello, "123456");
    // A recorded session replayed whole (same console hello) fails: the server's nonce is fresh.
    const Digest recorded = auth_proof(*secret, Side::Console, console_hello, server_hello);
    ServerHandshake server(hello_of(3, 8));
    server.on_hello(console_hello, secret);
    CHECK(!server.on_proof(recorded));
    // The server's own proof reflected back is not a console proof.
    ServerHandshake server2(server_hello);
    const auto out = server2.on_hello(console_hello, secret);
    CHECK(!server2.on_proof(digest_in(out[1])));
    // A flipped bit fails, and a second Hello is refused.
    Digest flipped = auth_proof(*secret, Side::Console, console_hello, server_hello);
    flipped[0] ^= 1;
    CHECK(!server2.on_proof(flipped));
    CHECK(types_of(server2.on_hello(console_hello, secret)) == std::vector<Type>{Type::Refused});
}

NNH_TEST(another_protocol_version_is_refused_on_both_sides) {
    Hello old = hello_of(1, 2);
    old.version = kProtocolVersion + 1;
    ServerHandshake server(hello_of(3, 4));
    CHECK(types_of(server.on_hello(old, std::nullopt)) == std::vector<Type>{Type::Refused});
    ClientHandshake console(hello_of(1, 2));
    CHECK(console.on_hello(old, std::nullopt) == ClientHandshake::Step::Failed);
    CHECK(console.failure().find("version") != std::string::npos);
}
