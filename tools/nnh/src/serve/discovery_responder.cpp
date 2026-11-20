#include "serve/discovery_responder.hpp"

#include "nnh_link/discovery.hpp"

namespace nnh::tool {

bool answer_discovery(const net::Socket& udp, const Id& server_id, const std::string& name, int timeout_ms) {
    char question[64];
    Ipv4 from;
    std::uint16_t port = 0;
    const long n = net::udp_receive(udp, question, sizeof question, timeout_ms, &from, &port);
    if (n <= 0 || !is_discovery_question(question, static_cast<std::size_t>(n)) || !is_local_network(from)) return false;
    const std::string answer = discovery_answer(server_id, name);
    return net::udp_send(udp, from, port, answer.data(), answer.size());
}

} // namespace nnh::tool
