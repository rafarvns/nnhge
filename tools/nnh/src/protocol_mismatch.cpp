// The message of a protocol refusal (protocol_mismatch.hpp).

#include "protocol_mismatch.hpp"

#include "nnh_link/messages.hpp"

namespace nnh::tool {

std::string protocol_mismatch_message(ProtocolSide side, std::optional<int> theirs, int ours,
                                      const std::string& nnh_version, const std::string& project_engine) {
    const std::string our_version = std::to_string(ours);
    if (side == ProtocolSide::Link) {
        return "NNH Link speaks link protocol version " + (theirs ? std::to_string(*theirs) : std::string("?")) +
               ", nnh " + nnh_version + " speaks " + our_version +
               ": use the NNH Link and the nnh of the same NNHGE release";
    }
    const std::string engine = project_engine.empty() ? "unknown" : project_engine;
    std::string what =
        theirs ? "the game speaks remote protocol version " + std::to_string(*theirs) + ", nnh " + nnh_version +
                     " speaks " + our_version + " (this project's engine: " + engine + ")"
               : "the game refused nnh's remote protocol version " + our_version + " (nnh " + nnh_version +
                     "; this project's engine: " + engine + ")";
    if (project_engine.empty()) {
        return what + ": use the nnh of the game's engine release, or rebuild the game with NNHGE " + nnh_version;
    }
    if (project_engine == nnh_version) {
        return what + ": rebuild the game (it was built with another engine than this project's)";
    }
    return what + ": use the nnh of NNHGE " + project_engine + ", or change the project's engine pin to " + nnh_version;
}

std::string serve_end_text(const std::string& end, const std::string& nnh_version) {
    static const std::string kPrefix = "the console speaks link protocol version ";
    if (end.rfind(kPrefix, 0) != 0) return end;
    const std::string number = end.substr(kPrefix.size());
    if (number.empty() || number.size() > 5 || number.find_first_not_of("0123456789") != std::string::npos) return end;
    return protocol_mismatch_message(ProtocolSide::Link, std::stoi(number), nnh::link::kProtocolVersion, nnh_version,
                                     "");
}

} // namespace nnh::tool
