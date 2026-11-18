#include "nnh_link/client_session.hpp"

#include <chrono>

namespace nnh::link {

namespace {

std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

void ClientSession::start(net::Socket connected) {
    connection_ = Connection(static_cast<net::Socket&&>(connected));
    last_activity_ms_ = now_ms();
    if (!connection_.send_frame(hs_.hello())) fail("can't reach the PC");
}

bool ClientSession::enter_code(const std::string& code) {
    if (state_ != State::NeedCode) return false;
    code_sent_ = true;
    state_ = State::Handshake;
    last_activity_ms_ = now_ms();
    if (!connection_.send_frame(hs_.pair(code))) fail("the PC went away");
    return state_ != State::Failed;
}

bool ClientSession::list_games() {
    if (state_ != State::Ready) return false;
    state_ = State::Busy;
    last_activity_ms_ = now_ms();
    if (!connection_.send(Type::ListGames, {})) fail("the PC went away");
    return state_ != State::Failed;
}

bool ClientSession::build(const BuildRequest& request, FileSink& sink) {
    if (state_ != State::Ready || !valid_game_name(request.game)) return false;
    state_ = State::Busy;
    build_ = Build::Building;
    lines_.clear();
    sink_ = &sink;
    file_size_ = received_ = 0;
    last_activity_ms_ = now_ms();
    if (!connection_.send(Type::Build, encode(request))) fail("the PC went away");
    return state_ != State::Failed;
}

bool ClientSession::launching(const std::string& game) {
    return state_ == State::Ready && connection_.send(Type::Launching, encode_text(game));
}

void ClientSession::cancel() {
    if (state_ != State::Failed) fail("cancelled");
}

void ClientSession::fail(const std::string& why) {
    if (build_ == Build::Receiving && sink_ != nullptr) sink_->finish(false); // drop the partial file
    if (build_ == Build::Building || build_ == Build::Receiving) build_ = Build::Failed;
    sink_ = nullptr;
    connection_.close();
    state_ = State::Failed;
    error_ = why;
}

int ClientSession::timeout_ms() const {
    if (state_ == State::NeedCode || state_ == State::Ready) return 0; // waiting for the user
    if (state_ == State::Handshake) return limits_.handshake_ms;
    return build_ == Build::Building ? limits_.build_ms : limits_.reply_ms;
}

ClientSession::State ClientSession::update(int wait_ms) {
    while (state_ != State::Failed) {
        Frame f;
        const auto status = connection_.receive(f, wait_ms);
        wait_ms = 0; // only the first read waits
        if (status == Connection::Status::Frame) {
            last_activity_ms_ = now_ms();
            if (connection_.secured()) {
                on_message(f);
            } else {
                on_handshake(f);
            }
            continue;
        }
        if (status == Connection::Status::Closed) {
            fail(build_ == Build::Receiving ? "the PC went away during the transfer" : "the PC closed the connection");
        } else if (status == Connection::Status::Broken) {
            fail("the PC sent a malformed or forged message");
        } else if (timeout_ms() > 0 && now_ms() - last_activity_ms_ > timeout_ms()) {
            fail("the PC stopped answering");
        }
        break;
    }
    return state_;
}

void ClientSession::on_handshake(const Frame& f) {
    using Step = ClientHandshake::Step;
    std::string reason;
    Digest d;
    Step step = Step::Wait;
    if (f.type == Type::Refused && decode_text(f.payload, reason)) {
        return fail(reason.empty() ? "the PC refused" : "the PC refused: " + reason);
    }
    if (!got_hello_) {
        Hello server;
        if (f.type != Type::Hello || !decode(f.payload, server)) return fail("the PC isn't nnh serve");
        got_hello_ = true;
        step = hs_.on_hello(server, store_.secret_of(server.id));
    } else if (f.type == Type::NeedPairing && f.payload.empty() && !paired_) {
        if (code_sent_) return; // already pairing (the code was typed before this frame was read)
        step = hs_.on_pairing_needed();
    } else if (f.type == Type::Paired && f.payload.empty() && code_sent_ && !paired_) {
        paired_ = true;
        step = hs_.on_paired();
    } else if (f.type == Type::Proof && decode_digest(f.payload, d)) {
        if (state_ == State::NeedCode || (code_sent_ && !paired_)) return; // a proof for a pairing we don't have
        step = hs_.on_server_proof(d);
    } else {
        return fail("unexpected message during the handshake");
    }
    if (step == Step::Failed) return fail(hs_.failure());
    if (step == Step::NeedCode) state_ = State::NeedCode;
    if (step != Step::SendProof) return;
    if (!connection_.send_frame(hs_.proof())) return fail("the PC went away");
    if (hs_.new_secret()) {
        store_.remember(hs_.server().id, *hs_.new_secret());
        paired_now_ = true;
    }
    connection_.secure(hs_.key(), Side::Console);
    state_ = State::Ready;
}

void ClientSession::on_message(const Frame& f) {
    std::string text;
    if (f.type == Type::Refused && decode_text(f.payload, text)) return fail("the PC refused: " + text);
    if (state_ != State::Busy) return fail("unexpected message from the PC");
    if (f.type == Type::Games && build_ != Build::Building && build_ != Build::Receiving) {
        if (!decode(f.payload, games_)) return fail("the PC sent a malformed game list");
        state_ = State::Ready;
    } else if (f.type == Type::BuildLine && build_ == Build::Building && decode_text(f.payload, text)) {
        lines_.push_back(text);
        if (lines_.size() > kMaxErrorLines) lines_.erase(lines_.begin());
    } else if (f.type == Type::BuildFailed && build_ == Build::Building && decode_lines(f.payload, lines_)) {
        build_ = Build::Failed;
        sink_ = nullptr;
        state_ = State::Ready;
    } else if (f.type == Type::BuildDone && build_ == Build::Building) {
        BuildResult result;
        if (!decode(f.payload, result)) return fail("the PC sent a malformed build result");
        if (!sink_->begin(result.size)) return fail("not enough space on the SD card");
        build_ = Build::Receiving;
        file_size_ = result.size;
        expected_ = result.sha256;
        hash_ = Sha256();
        if (file_size_ == 0) on_chunk({});
    } else if (f.type == Type::FileChunk && build_ == Build::Receiving) {
        on_chunk(f.payload);
    } else {
        fail("unexpected message from the PC");
    }
}

void ClientSession::on_chunk(const std::vector<std::uint8_t>& data) {
    if (data.size() > file_size_ - received_) return fail("the PC sent more than the game's size");
    if (!data.empty() && !sink_->write(data.data(), data.size())) return fail("can't write the game to the SD card");
    hash_.update(data.data(), data.size());
    received_ += data.size();
    if (received_ < file_size_) return;
    const Digest got = hash_.finish();
    if (!equal_constant_time(got.data(), expected_.data(), got.size())) {
        return fail("the game arrived damaged (SHA-256 mismatch)");
    }
    sink_->finish(true);
    sink_ = nullptr;
    build_ = Build::Received;
    state_ = State::Ready;
}

} // namespace nnh::link
