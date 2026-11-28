// The message of a protocol refusal (protocol_mismatch.hpp, spec 033 FR-22).

#include "protocol_mismatch.hpp"
#include "test.hpp"

#include <string>

using namespace nnh::tool;

namespace {

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

} // namespace

NNH_TEST(a_game_with_another_version_names_both_and_the_fix) {
    const std::string m = protocol_mismatch_message(ProtocolSide::Remote, 2, 1, "0.1.0", "0.2.0");
    CHECK(m == "the game speaks remote protocol version 2, nnh 0.1.0 speaks 1 (this project's engine: 0.2.0): use "
               "the nnh of NNHGE 0.2.0, or change the project's engine pin to 0.1.0");
}

NNH_TEST(a_game_that_refuses_the_hello_has_no_version_to_name) {
    const std::string m = protocol_mismatch_message(ProtocolSide::Remote, std::nullopt, 1, "0.1.0", "0.2.0");
    CHECK(has(m, "the game refused nnh's remote protocol version 1 (nnh 0.1.0; this project's engine: 0.2.0)"));
    CHECK(has(m, "use the nnh of NNHGE 0.2.0, or change the project's engine pin to 0.1.0"));
}

NNH_TEST(an_unknown_or_equal_project_engine_points_at_a_rebuild) {
    std::string m = protocol_mismatch_message(ProtocolSide::Remote, 2, 1, "0.1.0", "");
    CHECK(has(m, "(this project's engine: unknown)") && has(m, "use the nnh of the game's engine release"));
    m = protocol_mismatch_message(ProtocolSide::Remote, 2, 1, "0.1.0", "0.1.0");
    CHECK(has(m, "(this project's engine: 0.1.0)") && has(m, "rebuild the game"));
}

NNH_TEST(nnh_link_refusals_name_the_link_versions) {
    const std::string m = protocol_mismatch_message(ProtocolSide::Link, 2, 1, "0.1.0", "");
    CHECK(m == "NNH Link speaks link protocol version 2, nnh 0.1.0 speaks 1: use the NNH Link and the nnh of the same "
               "NNHGE release");
}
