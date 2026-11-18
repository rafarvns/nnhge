// The server and client sessions together on the loopback interface (spec 011 FR-1, FR-3, AC-2): pairing,
// reconnection without a code, list, build, file transfer, launch; and who is refused.

#include "session_fakes.hpp"
#include "test.hpp"

using namespace fakes;
using State = ClientSession::State;

NNH_TEST(pair_list_build_receive_and_launch) {
    Hooks hooks;
    hooks.file = make_file("session_game", 40000); // three chunks
    PairingGuard guard;
    Store store;
    Server server(hooks, guard, hello_of(0xAA, 1));
    ClientSession console(hello_of(0xC0, 2), store);
    console.start(server.connect());
    CHECK(settle(console) == State::NeedCode);
    CHECK(console.enter_code("123456") && settle(console) == State::Ready && console.paired_now());
    CHECK(console.list_games() && settle(console) == State::Ready);
    CHECK(console.games().size() == 2 && console.games()[1].name == "audio");
    Sink sink;
    CHECK(console.build(BuildRequest{"audio", false}, sink) && settle(console) == State::Ready);
    CHECK(console.build_state() == ClientSession::Build::Received && sink.kept && !sink.dropped);
    CHECK(sink.data.size() == 40000 && console.received() == 40000 && console.lines().size() == 2);
    CHECK(console.launching("audio"));
    console.cancel(); // leaves
    CHECK(server.end() == "the console left");
    CHECK(hooks.launched == "audio" && hooks.pairs.find(hello_of(0xC0, 0).id).has_value());
    CHECK(store.pairs.find(hello_of(0xAA, 0).id) == hooks.pairs.find(hello_of(0xC0, 0).id));
    std::remove(hooks.file.c_str());
}

NNH_TEST(a_paired_console_reconnects_without_a_code) {
    Hooks hooks;
    PairingGuard guard;
    Store store;
    {
        Server server(hooks, guard, hello_of(0xAA, 1));
        ClientSession console(hello_of(0xC0, 2), store);
        console.start(server.connect());
        settle(console);
        console.enter_code("123456");
        CHECK(settle(console) == State::Ready);
    }
    hooks.code = ""; // no code shown now
    Server server(hooks, guard, hello_of(0xAA, 3));
    ClientSession console(hello_of(0xC0, 4), store);
    console.start(server.connect());
    CHECK(settle(console) == State::Ready && !console.paired_now());
    CHECK(console.list_games() && settle(console) == State::Ready && console.games().size() == 2);
}

NNH_TEST(a_wrong_code_is_refused_and_nothing_is_remembered) {
    Hooks hooks;
    PairingGuard guard;
    Store store;
    Server server(hooks, guard, hello_of(0xAA, 1));
    ClientSession console(hello_of(0xC0, 2), store);
    console.start(server.connect());
    settle(console);
    console.enter_code("654321");
    CHECK(settle(console) == State::Failed && console.error().find("wrong code") != std::string::npos);
    CHECK(server.end() == "pairing refused: wrong code");
    CHECK(store.pairs.list.empty() && hooks.pairs.list.empty());
}

NNH_TEST(a_code_typed_before_need_pairing_arrives_still_pairs) {
    // Regression: Hello and NeedPairing are two frames; the console asks the code after Hello, and the
    // user may type it before NeedPairing is read. That late NeedPairing must not break the handshake.
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    bool server_ok = false;
    std::thread server([&] {
        Connection c(net::tcp_accept(listener, 5000));
        Frame f;
        Hello console;
        if (c.receive(f, 5000) != Connection::Status::Frame || !decode(f.payload, console)) return;
        ServerHandshake hs(hello_of(0xAA, 1));
        const auto first = hs.on_hello(console, std::nullopt); // {Hello, NeedPairing}
        c.send_frame(first[0]);
        Digest d;
        if (c.receive(f, 5000) != Connection::Status::Frame || !decode_digest(f.payload, d)) return;
        c.send_frame(first[1]); // late
        PairingGuard guard;
        for (const auto& out : hs.on_pair(d, "123456", guard)) c.send_frame(out);
        server_ok = c.receive(f, 5000) == Connection::Status::Frame && decode_digest(f.payload, d) && hs.on_proof(d);
        c.receive(f, 5000); // stay until the console leaves
    });
    Store store;
    ClientSession console(hello_of(0xC0, 2), store);
    console.start(net::tcp_connect(kLoopback, listener.local_port(), 5000));
    CHECK(settle(console) == State::NeedCode);
    CHECK(console.enter_code("123456") && settle(console) == State::Ready);
    console.cancel();
    server.join();
    CHECK(server_ok);
}

NNH_TEST(a_console_the_server_forgot_pairs_again) {
    Hooks hooks;
    PairingGuard guard;
    Store store;
    store.pairs.put(hello_of(0xAA, 0).id, Digest{}); // the console still has an old pairing
    Server server(hooks, guard, hello_of(0xAA, 1));
    ClientSession console(hello_of(0xC0, 2), store);
    console.start(server.connect());
    CHECK(settle(console) == State::NeedCode);
    CHECK(console.enter_code("123456") && settle(console) == State::Ready);
    CHECK(store.pairs.find(hello_of(0xAA, 0).id) != Digest{});
}

NNH_TEST(a_console_that_forgot_the_server_pairs_again) {
    Hooks hooks;
    hooks.pairs.put(hello_of(0xC0, 0).id, Digest{}); // the server still knows the console
    PairingGuard guard;
    Store store;
    Server server(hooks, guard, hello_of(0xAA, 1));
    ClientSession console(hello_of(0xC0, 2), store);
    console.start(server.connect());
    CHECK(settle(console) == State::NeedCode); // the server's proof is ignored: no secret to check it
    CHECK(console.enter_code("123456") && settle(console) == State::Ready);
}

NNH_TEST(an_unpaired_console_cannot_start_a_build) {
    Hooks hooks;
    hooks.pairs.put(hello_of(0xC0, 0).id, Digest{}); // paired with a secret this impostor doesn't have
    PairingGuard guard;
    Server server(hooks, guard, hello_of(0xAA, 1));
    Connection raw(server.connect());
    raw.send(Type::Hello, encode(hello_of(0xC0, 9)));
    Digest guess;
    guess.fill(7);
    raw.send(Type::Proof, encode_digest(guess));
    raw.secure(guess, Side::Console);
    raw.send(Type::Build, encode(BuildRequest{"hello", false}));
    CHECK(server.end() == "a console that isn't paired tried to connect" && hooks.builds == 0);
    // A Build before any handshake isn't even a hello.
    Server server2(hooks, guard, hello_of(0xAA, 1));
    Connection raw2(server2.connect());
    raw2.send(Type::Build, encode(BuildRequest{"hello", false}));
    CHECK(server2.end() == "the peer isn't NNH Link" && hooks.builds == 0);
}

NNH_TEST(the_console_refuses_an_impostor_server) {
    Hooks impostor;
    impostor.file = make_file("session_impostor", 100);
    Digest stolen_guess;
    stolen_guess.fill(1);
    impostor.pairs.put(hello_of(0xC0, 0).id, stolen_guess); // claims to know the console
    PairingGuard guard;
    Store store;
    Digest real;
    real.fill(2);
    store.pairs.put(hello_of(0xAA, 0).id, real); // paired with the real server, same id
    Server server(impostor, guard, hello_of(0xAA, 1));
    ClientSession console(hello_of(0xC0, 2), store);
    console.start(server.connect());
    CHECK(settle(console) == State::Failed && console.error().find("prove") != std::string::npos);
    Sink sink;
    CHECK(!console.build(BuildRequest{"hello", false}, sink) && !sink.begun);
    CHECK(store.pairs.find(hello_of(0xAA, 0).id) == real); // the pairing isn't replaced
    std::remove(impostor.file.c_str());
}

NNH_TEST(five_wrong_codes_lock_pairing_across_connections) {
    Hooks hooks;
    PairingGuard guard;
    Store store;
    for (int i = 0; i < 5; ++i) {
        Server server(hooks, guard, hello_of(0xAA, 1));
        ClientSession console(hello_of(0xC0, 2), store);
        console.start(server.connect());
        settle(console);
        console.enter_code("000000");
        settle(console);
    }
    Server server(hooks, guard, hello_of(0xAA, 1));
    ClientSession console(hello_of(0xC0, 2), store);
    console.start(server.connect());
    settle(console);
    console.enter_code("123456"); // right, but too late
    CHECK(settle(console) == State::Failed && console.error().find("too many") != std::string::npos);
}
