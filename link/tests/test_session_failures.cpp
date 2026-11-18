// Session failures (spec 011 FR-5): a failed build, the PC gone mid-file, a damaged file, no SD space,
// silence (timeouts) and B (cancel). Nothing waits forever and a partial file is always dropped.

#include "session_fakes.hpp"
#include "test.hpp"

using namespace fakes;
using State = ClientSession::State;

namespace {

/// A console paired with a server, Ready.
struct Paired {
    Hooks hooks;
    PairingGuard guard;
    Store store;
    Server server;
    ClientSession console;
    explicit Paired(ClientLimits limits = {}, std::uint8_t nonce = 1)
        : server(hooks, guard, hello_of(0xAA, nonce)), console(hello_of(0xC0, 2), store, limits) {
        console.start(server.connect());
        settle(console);
        console.enter_code("123456");
        settle(console);
    }
};

/// A server that pairs properly, then answers a Build with `size` bytes announced but only `send` sent,
/// optionally with a wrong hash, and hangs up.
std::string lying_server(net::Socket s, std::uint64_t size, std::size_t send, bool wrong_hash) {
    Connection c(static_cast<net::Socket&&>(s));
    Frame f;
    Hello console;
    if (c.receive(f, 5000) != Connection::Status::Frame || !decode(f.payload, console)) return "no hello";
    ServerHandshake hs(hello_of(0xAA, 5));
    for (const auto& out : hs.on_hello(console, std::nullopt)) c.send_frame(out);
    PairingGuard guard;
    Digest d;
    if (c.receive(f, 5000) != Connection::Status::Frame || !decode_digest(f.payload, d)) return "no pair";
    for (const auto& out : hs.on_pair(d, "123456", guard)) c.send_frame(out);
    if (c.receive(f, 5000) != Connection::Status::Frame || !decode_digest(f.payload, d) || !hs.on_proof(d)) return "no proof";
    c.secure(hs.key(), Side::Server);
    if (c.receive(f, 5000) != Connection::Status::Frame || f.type != Type::Build) return "no build";
    std::vector<std::uint8_t> data(send, 0x42);
    Digest hash = sha256(data.data(), data.size());
    if (wrong_hash) hash[0] ^= 1;
    c.send(Type::BuildDone, encode(BuildResult{size, hash}));
    c.send(Type::FileChunk, data);
    return "done";
}

State build_from_lying_server(std::uint64_t size, std::size_t send, bool wrong_hash, Sink& sink, std::string& error) {
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    std::thread t([&] { lying_server(net::tcp_accept(listener, 5000), size, send, wrong_hash); });
    Store store;
    ClientSession console(hello_of(0xC0, 2), store);
    console.start(net::tcp_connect(kLoopback, listener.local_port(), 5000));
    settle(console);
    console.enter_code("123456");
    settle(console);
    console.build(BuildRequest{"hello", false}, sink);
    const State s = settle(console);
    t.join();
    error = console.error();
    return s;
}

} // namespace

NNH_TEST(a_failed_build_shows_the_last_lines_and_the_session_goes_on) {
    Paired p;
    p.hooks.build_ok = false;
    for (int i = 0; i < 25; ++i) p.hooks.output.push_back("main.cpp:" + std::to_string(i) + ": error");
    Sink sink;
    CHECK(p.console.build(BuildRequest{"hello", false}, sink) && settle(p.console) == State::Ready);
    CHECK(p.console.build_state() == ClientSession::Build::Failed && !sink.begun);
    CHECK(p.console.lines().size() == kMaxErrorLines && p.console.lines().back() == "main.cpp:24: error");
    p.hooks.build_ok = true;
    p.hooks.output = {"ok"};
    p.hooks.file = make_file("session_retry", 10);
    CHECK(p.console.build(BuildRequest{"hello", false}, sink) && settle(p.console) == State::Ready);
    CHECK(p.console.build_state() == ClientSession::Build::Received && sink.kept);
    std::remove(p.hooks.file.c_str());
}

NNH_TEST(the_pc_going_away_mid_file_drops_the_partial_file) {
    Sink sink;
    std::string error;
    CHECK(build_from_lying_server(100000, 16384, false, sink, error) == State::Failed);
    CHECK(error.find("during the transfer") != std::string::npos && sink.begun && sink.dropped && !sink.kept);
}

NNH_TEST(a_damaged_file_is_refused) {
    Sink sink;
    std::string error;
    CHECK(build_from_lying_server(1000, 1000, true, sink, error) == State::Failed);
    CHECK(error.find("damaged") != std::string::npos && sink.dropped && !sink.kept);
}

NNH_TEST(more_data_than_announced_is_refused) {
    Sink sink;
    std::string error;
    CHECK(build_from_lying_server(1000, 2000, false, sink, error) == State::Failed);
    CHECK(error.find("more than") != std::string::npos && sink.data.empty() && sink.dropped);
}

NNH_TEST(a_long_build_output_keeps_only_the_last_lines) {
    Paired p;
    p.hooks.output.clear();
    for (int i = 0; i < 30; ++i) p.hooks.output.push_back("step " + std::to_string(i));
    p.hooks.file = make_file("session_lines", 10);
    Sink sink;
    CHECK(p.console.build(BuildRequest{"hello", false}, sink) && settle(p.console) == State::Ready);
    CHECK(p.console.lines().size() == kMaxErrorLines && p.console.lines().front() == "step 10");
    std::remove(p.hooks.file.c_str());
}

NNH_TEST(no_space_on_the_sd_card_stops_the_transfer) {
    Paired p;
    p.hooks.file = make_file("session_big", 5000);
    Sink sink;
    sink.space = 4000;
    p.console.build(BuildRequest{"hello", false}, sink);
    CHECK(settle(p.console) == State::Failed && p.console.error().find("space") != std::string::npos);
    CHECK(sink.data.empty() && !sink.kept);
    std::remove(p.hooks.file.c_str());
}

NNH_TEST(a_silent_pc_times_out) {
    ClientLimits limits;
    limits.build_ms = 300;
    Paired p(limits);
    p.hooks.silent_ms = 1500; // builds without a word
    Sink sink;
    const auto start = std::chrono::steady_clock::now();
    p.console.build(BuildRequest{"hello", false}, sink);
    CHECK(settle(p.console) == State::Failed && p.console.error().find("stopped answering") != std::string::npos);
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1400));
}

NNH_TEST(a_console_that_never_answers_times_out_on_the_server) {
    Hooks hooks;
    PairingGuard guard;
    ServeLimits limits;
    limits.handshake_ms = 200;
    Server server(hooks, guard, hello_of(0xAA, 1), limits);
    net::Socket silent = server.connect(); // connects and says nothing
    CHECK(server.end() == "the console stopped answering before saying hello");
}

NNH_TEST(cancel_stops_the_build_on_the_pc) {
    Paired p;
    p.hooks.output.assign(100, "still compiling");
    p.hooks.line_delay_ms = 10;
    Sink sink;
    p.console.build(BuildRequest{"hello", false}, sink);
    while (p.console.lines().empty() && p.console.update(10) == State::Busy) {
    }
    p.console.cancel();
    CHECK(p.console.state() == State::Failed && p.console.error() == "cancelled" && !sink.begun);
    CHECK(p.server.end() == "the console left during the build" && p.hooks.lines_sent < 100);
}
