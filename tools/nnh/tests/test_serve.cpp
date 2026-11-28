// nnh serve's parts (spec 011): the pairings file, the pairing code, the discovery answer, the pairing side
// of ServeHooks, and running a command with its output line by line.

#include <cstdio>
#include <filesystem>
#include <string>

#include "nnh_link/discovery.hpp"
#include "repo.hpp"
#include "serve/discovery_responder.hpp"
#include "serve/pairing_codes.hpp"
#include "serve/pairs_file.hpp"
#include "serve/serve_hooks.hpp"
#include "protocol_mismatch.hpp"
#include "test.hpp"

using namespace nnh::tool;

namespace {

Id id_of(std::uint8_t v) {
    Id id;
    id.fill(v);
    return id;
}

Digest secret_of(std::uint8_t v) {
    Digest d;
    d.fill(v);
    return d;
}

std::string tmp(const char* name) { return std::string(NNH_TOOL_TEST_TMP) + "/" + name; }

} // namespace

NNH_TEST(pairs_round_trip_and_skip_junk) {
    const PairList pairs = {{id_of(1), secret_of(2)}, {id_of(3), secret_of(4)}};
    CHECK(parse_pairs(format_pairs(pairs)) == pairs);
    const std::string text = format_pairs({{id_of(1), secret_of(9)}}) + "junk\nabc=def\n" +
                             to_hex(id_of(1).data(), 16) + "=" + to_hex(secret_of(5).data(), 32) + "\r\n";
    const PairList parsed = parse_pairs(text);
    CHECK(parsed.size() == 1 && parsed[0].second == secret_of(5)); // the later line wins
    CHECK(pairs_path("C:/Users/x/AppData/Roaming/nnhge/nnh.ini") == "C:/Users/x/AppData/Roaming/nnhge/nnh-pairs.ini");
    CHECK(pairs_path("").empty());
}

NNH_TEST(pairs_file_saves_and_loads) {
    const std::string path = tmp("pairs_test/nnh-pairs.ini");
    std::filesystem::remove_all(tmp("pairs_test"));
    CHECK(load_pairs(path).empty());
    const PairList pairs = {{id_of(7), secret_of(8)}};
    CHECK(save_pairs(path, pairs) && load_pairs(path) == pairs);
    CHECK(save_pairs(path, {}) && load_pairs(path).empty());
#ifndef _WIN32
    const auto perms = std::filesystem::status(path).permissions();
    CHECK((perms & (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) == std::filesystem::perms::none);
#endif
}

NNH_TEST(pairing_codes_expire_after_two_minutes) {
    PairingCodes codes;
    CHECK(codes.current(0).empty());
    CHECK(codes.renew(1000, 42) == "000042");
    CHECK(codes.current(1000 + PairingCodes::kValidMs - 1) == "000042");
    CHECK(codes.current(1000 + PairingCodes::kValidMs).empty());
    codes.renew(5000, 7);
    codes.spend();
    CHECK(codes.current(5001).empty());
}

NNH_TEST(discovery_questions_get_an_answer_with_the_server_id) {
    const Ipv4 loopback{{127, 0, 0, 1}};
    net::Socket server = net::udp_open(loopback, 0);
    net::Socket client = net::udp_open(loopback, 0);
    CHECK(server.valid() && client.valid());
    CHECK(net::udp_send(client, loopback, server.local_port(), "NNHLINK?1", 9));
    CHECK(answer_discovery(server, id_of(0xAB), "RAFA-PC", 2000));
    char answer[128];
    const long n = net::udp_receive(client, answer, sizeof answer, 2000);
    Id id;
    std::string name;
    CHECK(n > 0 && nnh::link::parse_discovery_answer(answer, static_cast<std::size_t>(n), id, name));
    CHECK(id == id_of(0xAB) && name == "RAFA-PC");
    CHECK(net::udp_send(client, loopback, server.local_port(), "HELLO", 5));
    CHECK(!answer_discovery(server, id_of(0xAB), "RAFA-PC", 2000)); // not a question: no answer
    CHECK(!answer_discovery(server, id_of(0xAB), "RAFA-PC", 50));   // nothing came
}

NNH_TEST(serve_hooks_show_a_code_to_new_consoles_and_remember_pairings) {
    const std::string path = tmp("hooks_test/nnh-pairs.ini");
    std::filesystem::remove_all(tmp("hooks_test"));
    std::int64_t now = 0;
    std::uint32_t next_random = 123456;
    ServeHooks hooks("", false, path, [&] { return now; }, [&] { return next_random; });
    CHECK(hooks.pairing_code().empty());
    CHECK(!hooks.secret_of(id_of(1)).has_value() && hooks.pairing_code() == "123456"); // a new code
    next_random = 999;
    CHECK(!hooks.secret_of(id_of(2)).has_value() && hooks.pairing_code() == "123456"); // still valid: kept
    now = PairingCodes::kValidMs + 1;
    CHECK(hooks.pairing_code().empty());
    CHECK(!hooks.secret_of(id_of(1)).has_value() && hooks.pairing_code() == "000999"); // expired: renewed
    hooks.remember(id_of(1), secret_of(5));
    next_random = 31;
    CHECK(hooks.pairing_code().empty()); // spent: refused this time...
    CHECK(hooks.pairing_code() == "000031"); // ...and a new code is shown for the next try
    CHECK(hooks.secret_of(id_of(1)) == secret_of(5));
    ServeHooks reloaded("", false, path, [&] { return now; }, [&] { return next_random; });
    CHECK(reloaded.secret_of(id_of(1)) == secret_of(5));
    hooks.launching("audio");
    CHECK(hooks.take_launch() == "audio" && hooks.take_launch().empty());
    CHECK(short_id(id_of(0xAB)) == "abababab");
}

NNH_TEST(shell_output_arrives_line_by_line_and_can_be_stopped) {
    const Shell shell = host_shell(false);
    std::vector<std::string> lines;
    const int status = run_shell_lines("echo one&& echo two", shell, [&](const std::string& l) {
        lines.push_back(l);
        return true;
    });
    CHECK(status == 0 && lines.size() == 2 && lines[0].rfind("one", 0) == 0 && lines[1].rfind("two", 0) == 0);
    lines.clear();
    const int stopped = run_shell_lines("echo one&& echo two", shell, [&](const std::string& l) {
        lines.push_back(l);
        return false;
    });
    CHECK(stopped == -1 && lines.size() == 1);
    CHECK(run_shell_lines("exit 3", shell, [](const std::string&) { return true; }) == 3);
}

NNH_TEST(serve_names_the_versions_when_nnh_link_speaks_another_protocol) { // spec 033 FR-22
    const std::string m = nnh::tool::serve_end_text("the console speaks link protocol version 2", "0.1.0");
    CHECK(m == "NNH Link speaks link protocol version 2, nnh 0.1.0 speaks 1: use the NNH Link and the nnh of the same "
               "NNHGE release");
    CHECK(nnh::tool::serve_end_text("the console left", "0.1.0") == "the console left");
    CHECK(nnh::tool::serve_end_text("the console speaks link protocol version x", "0.1.0") ==
          "the console speaks link protocol version x");
}
