// Talking to the console (console_link.hpp, log_listener.hpp, file_server.hpp; spec 010) against fakes on the
// loopback interface: a netloader that checks every byte nnh sends, a game that writes its log, and an FBI
// that downloads the URL it receives.

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "console_link.hpp"
#include "file_server.hpp"
#include "log_listener.hpp"
#include "net.hpp"
#include "netloader.hpp"
#include "stored_inflate.hpp"
#include "test.hpp"

using namespace nnh::tool;

namespace {

const Ipv4 kLoopback{{127, 0, 0, 1}};

std::uint32_t le32(const std::uint8_t* b) { return b[0] | b[1] << 8 | b[2] << 16 | std::uint32_t(b[3]) << 24; }

void send_status(net::Socket& s, std::int32_t status) {
    const std::uint8_t b[4] = {std::uint8_t(status), std::uint8_t(status >> 8), std::uint8_t(status >> 16),
                               std::uint8_t(status >> 24)};
    s.send_all(b, 4);
}

/// What the fake netloader received.
struct Received {
    std::string name;
    std::vector<std::uint8_t> file;
    std::vector<std::string> args;
    std::size_t biggest_chunk = 0;
    bool ok = false;
};

/// A fake Homebrew Launcher netloader, like the real one (netloader.c): answers `first_status` to the
/// header, reads chunks until the zlib stream ends, answers `second_status`, reads the arguments.
void fake_netloader(net::Socket& listener, Received& r, std::int32_t first_status, std::int32_t second_status = 0) {
    net::Socket s = net::tcp_accept(listener, 5000);
    std::uint8_t b4[4];
    if (!s.valid() || !s.recv_all(b4, 4)) return;
    std::string name(le32(b4), '\0');
    if (!s.recv_all(name.data(), name.size()) || !s.recv_all(b4, 4)) return;
    r.name = name;
    const std::uint32_t file_size = le32(b4);
    send_status(s, first_status);
    if (first_status < 0) return;
    std::vector<std::uint8_t> stream;
    for (;;) {
        if (!s.recv_all(b4, 4)) return;
        const std::uint32_t n = le32(b4);
        if (n > r.biggest_chunk) r.biggest_chunk = n;
        if (n == 0 || n > 1u << 20) return;
        std::vector<std::uint8_t> chunk(n);
        if (!s.recv_all(chunk.data(), n)) return;
        stream.insert(stream.end(), chunk.begin(), chunk.end());
        const auto state = test::inflate_stored(stream, r.file);
        if (state == test::Inflate::Bad) return;
        if (state == test::Inflate::Done) break;
    }
    if (r.file.size() != file_size) return;
    send_status(s, second_status);
    if (second_status < 0) return;
    if (!s.recv_all(b4, 4)) return;
    std::string args(le32(b4), '\0');
    if (!s.recv_all(args.data(), args.size())) return;
    for (std::size_t at = 0; at < args.size();) {
        const std::size_t end = args.find('\0', at);
        r.args.push_back(args.substr(at, end - at));
        at = end + 1;
    }
    r.ok = true;
}

void collect(const std::string& line, void* context) { static_cast<std::vector<std::string>*>(context)->push_back(line); }

std::string temp_file(const char* name, const std::vector<std::uint8_t>& bytes) {
    const std::string path = std::string(NNH_TOOL_TEST_TMP) + "/" + name;
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f != nullptr) {
        std::fwrite(bytes.data(), 1, bytes.size(), f);
        std::fclose(f);
    }
    return path;
}

/// GETs `path` from 127.0.0.1:`port`: the status line and the body.
std::string http_get(std::uint16_t port, const std::string& path, std::string& body, const char* method = "GET") {
    net::Socket s = net::tcp_connect(kLoopback, port, 2000);
    const std::string request = std::string(method) + " " + path + " HTTP/1.1\r\nHost: x\r\n\r\n";
    s.send_all(request.data(), request.size());
    std::string response;
    char buffer[4096];
    for (long n; (n = s.recv_some(buffer, sizeof buffer)) > 0;) response.append(buffer, static_cast<std::size_t>(n));
    const std::size_t blank = response.find("\r\n\r\n");
    body = blank == std::string::npos ? "" : response.substr(blank + 4);
    return response.substr(0, response.find("\r\n"));
}

} // namespace

NNH_TEST(discovery_finds_the_console_that_answers) {
    net::Socket fake = net::udp_open(kLoopback, 0);
    net::Socket probe = net::udp_open(kLoopback, 0); // a free port for the reply
    const std::uint16_t reply_port = probe.local_port();
    probe.close();
    std::thread netloader([&] {
        char buffer[16];
        Ipv4 from;
        const long n = net::udp_receive(fake, buffer, sizeof buffer, 5000, &from);
        // Like the launcher: answer to the sender's address, on the netloader's reply port.
        if (n == 7 && std::memcmp(buffer, "3dsboot", 7) == 0) net::udp_send(fake, from, reply_port, "boot3ds", 7);
    });
    const auto found = discover_console(kLoopback, fake.local_port(), reply_port, 3, 1000);
    netloader.join();
    CHECK(found && *found == kLoopback);
}

NNH_TEST(discovery_without_a_console_gives_up) {
    net::Socket silent = net::udp_open(kLoopback, 0);
    CHECK(!discover_console(kLoopback, silent.local_port(), 0, 2, 50).has_value());
}

NNH_TEST(a_3dsx_reaches_the_netloader_intact) {
    std::vector<std::uint8_t> file(100000);
    for (std::size_t i = 0; i < file.size(); ++i) file[i] = static_cast<std::uint8_t>(i * 31 + (i >> 9));
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    Received r;
    std::thread console([&] { fake_netloader(listener, r, 0); });
    const std::string error = send_3dsx(kLoopback, listener.local_port(), "hello.3dsx", file, {"--demo"});
    console.join();
    CHECK(error.empty() && r.ok);
    CHECK(r.name == "hello.3dsx" && r.file == file);
    CHECK(r.biggest_chunk > 0 && r.biggest_chunk <= kMaxNetloaderChunk);
    CHECK(r.args.size() == 2 && r.args[0] == "sdmc:/3ds/hello.3dsx" && r.args[1] == "--demo");
}

NNH_TEST(the_netloader_refusing_the_file_is_reported) {
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    Received r;
    std::thread console([&] { fake_netloader(listener, r, -1); });
    const std::string error = send_3dsx(kLoopback, listener.local_port(), "hello.3dsx", {1, 2, 3}, {});
    console.join();
    CHECK(error.find("couldn't create") != std::string::npos);
    CHECK(!send_3dsx(kLoopback, listener.local_port(), "../x.3dsx", {1}, {}).empty()); // bad name: not sent
}

NNH_TEST(the_netloader_failing_to_write_the_file_is_reported) {
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    Received r;
    std::thread console([&] { fake_netloader(listener, r, 0, -1); });
    const std::string error = send_3dsx(kLoopback, listener.local_port(), "hello.3dsx", {1, 2, 3}, {});
    console.join();
    CHECK(error.find("didn't confirm") != std::string::npos);
}

NNH_TEST(no_netloader_is_reported) {
    net::Socket probe = net::tcp_listen(kLoopback, 0);
    const std::uint16_t port = probe.local_port();
    probe.close();
    CHECK(send_3dsx(kLoopback, port, "hello.3dsx", {1}, {}, 300).find("netloader") != std::string::npos);
}

NNH_TEST(the_game_log_arrives_line_by_line) {
    LogListener logs;
    CHECK(logs.open(kLoopback, 0));
    std::thread game([&] {
        net::Socket s = net::tcp_connect(kLoopback, logs.port(), 2000);
        const char text[] = "[f0        0.000s] I app    started\r\nsecond\n\nlast without newline";
        s.send_all(text, sizeof text - 1);
    });
    CHECK(logs.wait_for_game(2000));
    std::vector<std::string> lines;
    logs.read_lines(collect, &lines);
    game.join();
    CHECK(lines.size() == 4 && lines[0] == "[f0        0.000s] I app    started" && lines[1] == "second");
    CHECK(lines[2].empty() && lines[3] == "last without newline");
}

NNH_TEST(no_game_connection_times_out) {
    LogListener logs;
    CHECK(logs.open(kLoopback, 0));
    CHECK(!logs.wait_for_game(50));
}

NNH_TEST(the_file_server_sends_only_its_files) {
    const std::vector<std::uint8_t> bytes = {'C', 'I', 'A', 0, 1, 2};
    FileServer server;
    CHECK(server.start(kLoopback, 0, {{"/game.cia", temp_file("nnh_test_game.cia", bytes)}}));
    std::string body;
    CHECK(http_get(server.port(), "/game.cia", body) == "HTTP/1.0 200 OK");
    CHECK(body == std::string("CIA\0\1\2", 6));
    CHECK(http_get(server.port(), "/other.cia", body).find("404") != std::string::npos);
    CHECK(http_get(server.port(), "/../game.cia", body).find("404") != std::string::npos);
    CHECK(http_get(server.port(), "/game.cia", body, "POST").find("404") != std::string::npos);
    CHECK(server.served() == 1);
    server.stop();
}

NNH_TEST(fbi_downloads_the_urls_it_is_sent) {
    const std::vector<std::uint8_t> bytes(70000, 0x5A);
    FileServer server;
    CHECK(server.start(kLoopback, 0, {{"/audio.cia", temp_file("nnh_test_audio.cia", bytes)}}));
    net::Socket fbi_port = net::tcp_listen(kLoopback, 0);
    std::string received_url;
    std::string downloaded;
    std::thread fbi([&] {
        net::Socket s = net::tcp_accept(fbi_port, 5000);
        std::uint8_t b4[4];
        if (!s.valid() || !s.recv_all(b4, 4)) return;
        received_url.resize(std::uint32_t(b4[0]) << 24 | b4[1] << 16 | b4[2] << 8 | b4[3]);
        if (!s.recv_all(received_url.data(), received_url.size())) return;
        const std::size_t path = received_url.find('/', 7); // after "http://host:port"
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // FBI takes its time
        http_get(server.port(), received_url.substr(path), downloaded);
        s.send_all("\0", 1); // FBI is done
    });
    const std::string url = "http://127.0.0.1:" + std::to_string(server.port()) + "/audio.cia";
    const std::string error = fbi_send_urls(kLoopback, fbi_port.local_port(), {url}, 5000);
    const int served = server.served();
    server.stop(); // like nnh install: done when FBI says so
    fbi.join();
    CHECK(error.empty() && received_url == url);
    CHECK(downloaded.size() == bytes.size() && served == 1);
}

NNH_TEST(no_fbi_is_reported) {
    net::Socket probe = net::tcp_listen(kLoopback, 0);
    const std::uint16_t port = probe.local_port();
    probe.close();
    CHECK(fbi_send_urls(kLoopback, port, {"http://x/y"}, 100, 300).find("Receive URLs") != std::string::npos);
}
