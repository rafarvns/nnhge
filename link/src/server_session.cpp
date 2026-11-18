#include "nnh_link/server_session.hpp"

#include <cstdio>

#include "nnh_link/connection.hpp"

namespace nnh::link {

namespace {

constexpr long kMaxGameFile = 64L * 1024 * 1024; // as BuildResult's decoder accepts

std::string why(Connection::Status status, const std::string& when) {
    const std::string tail = when.empty() ? "" : " " + when;
    switch (status) {
    case Connection::Status::Closed: return "the console left" + tail;
    case Connection::Status::Waiting: return "the console stopped answering" + tail;
    default: return "the console sent a malformed or forged message" + tail;
    }
}

/// Runs the handshake. @return "" when the console is authenticated, else why not.
std::string handshake(Connection& c, ServerHandshake& hs, const Hello& console, ServerHooks& hooks,
                      PairingGuard& guard, int timeout_ms) {
    for (const auto& out : hs.on_hello(console, hooks.secret_of(console.id))) c.send_frame(out);
    if (console.version != kProtocolVersion) return "the console speaks another protocol version";
    Frame f;
    while (!hs.done()) {
        const auto status = c.receive(f, timeout_ms);
        if (status != Connection::Status::Frame) return why(status, "during the handshake");
        Digest d;
        if (!decode_digest(f.payload, d)) return "malformed handshake message";
        if (f.type == Type::Pair) {
            const auto out = hs.on_pair(d, hooks.pairing_code(), guard);
            for (const auto& frame : out) c.send_frame(frame);
            if (out.size() == 1) { // Refused{reason}
                std::string reason;
                FrameReader r;
                bool error = false;
                r.feed(out[0].data(), out[0].size());
                if (auto rf = r.next(error)) decode_text(rf->payload, reason);
                return "pairing refused: " + reason;
            }
            hooks.remember(console.id, *hs.new_secret());
        } else if (f.type == Type::Proof) {
            if (!hs.on_proof(d)) {
                c.send(Type::Refused, encode_text("this PC doesn't know your console: pair again"));
                return "a console that isn't paired tried to connect";
            }
        } else {
            return "unexpected message during the handshake";
        }
    }
    return "";
}

/// Builds and sends the result. @return false if the connection is gone.
bool build_and_send(Connection& c, ServerHooks& hooks, const BuildRequest& request) {
    bool alive = true;
    const BuildOutput out = hooks.build(request, [&](const std::string& line) {
        alive = alive && c.send(Type::BuildLine, encode_text(line));
        return alive;
    });
    if (!alive) return false;
    if (!out.ok) return c.send(Type::BuildFailed, encode_lines(out.lines));
    std::FILE* file = std::fopen(out.file.c_str(), "rb");
    long size = -1;
    if (file != nullptr && std::fseek(file, 0, SEEK_END) == 0) size = std::ftell(file);
    if (size < 0 || size > kMaxGameFile) {
        if (file != nullptr) std::fclose(file);
        return c.send(Type::BuildFailed, encode_lines({"nnh serve can't read " + out.file}));
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    std::rewind(file);
    const bool read = std::fread(data.data(), 1, data.size(), file) == data.size();
    std::fclose(file);
    if (!read) return c.send(Type::BuildFailed, encode_lines({"nnh serve can't read " + out.file}));
    if (!c.send(Type::BuildDone, encode(BuildResult{data.size(), sha256(data.data(), data.size())}))) return false;
    for (std::size_t at = 0; at < data.size(); at += kMaxFileChunk) {
        const std::size_t n = data.size() - at < kMaxFileChunk ? data.size() - at : kMaxFileChunk;
        if (!c.send(Type::FileChunk, std::vector<std::uint8_t>(data.begin() + at, data.begin() + at + n))) return false;
    }
    return true;
}

} // namespace

std::string serve_console(net::Socket socket, const Hello& mine, ServerHooks& hooks, PairingGuard& guard,
                          const ServeLimits& limits) {
    Connection c(static_cast<net::Socket&&>(socket));
    Frame f;
    const auto first = c.receive(f, limits.handshake_ms);
    if (first != Connection::Status::Frame) return why(first, "before saying hello");
    Hello console;
    if (f.type != Type::Hello || !decode(f.payload, console)) return "the peer isn't NNH Link";
    ServerHandshake hs(mine);
    const std::string refused = handshake(c, hs, console, hooks, guard, limits.handshake_ms);
    if (!refused.empty()) return refused;
    c.secure(hs.key(), Side::Server);

    for (;;) {
        const auto status = c.receive(f, limits.idle_ms);
        if (status != Connection::Status::Frame) return why(status, "");
        BuildRequest request;
        std::string game;
        if (f.type == Type::ListGames && f.payload.empty()) {
            if (!c.send(Type::Games, encode(hooks.games()))) return "the console left";
        } else if (f.type == Type::Build && decode(f.payload, request)) {
            if (!build_and_send(c, hooks, request)) return "the console left during the build";
        } else if (f.type == Type::Launching && decode_text(f.payload, game) && valid_game_name(game)) {
            hooks.launching(game);
        } else {
            return "unexpected message from the console";
        }
    }
}

} // namespace nnh::link
