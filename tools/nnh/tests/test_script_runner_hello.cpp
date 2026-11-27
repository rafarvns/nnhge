// Script runner: the Hello answered with another protocol version or refused (remote/script_runner_hello.cpp,
// spec 033 FR-22), against the fake console of fake_console.hpp.

#include <string>

#include "fake_console.hpp"
#include "remote/script_runner.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
using nnh::tool::test::FakeConsole;
namespace wire = nnh::detail::remote;

namespace {

ScriptRunner runner_for(const std::string& text) {
    return ScriptRunner(parse_script(text, "t.nnhs").script, RunnerOptions{.interactive = false, .quit_at_end = false});
}

} // namespace

NNH_TEST(a_game_answering_another_version_gets_both_versions_and_the_fix) {
    ScriptRunner r = runner_for("wait 1\n");
    r.set_versions("0.1.0", "0.2.0");
    r.start();
    std::uint8_t out[wire::kMaxFixedRecord];
    wire::encode_hello_record(1, 0, "Development", out);
    out[wire::kRecordHeaderSize] = 2; // version 2
    FakeConsole c(r);
    c.to_pc(out, wire::kRecordHeaderSize + wire::kHelloPayload);
    CHECK(r.verdict() == Verdict::Error && r.exit_code() == 2);
    CHECK(c.noted("the game speaks remote protocol version 2, nnh 0.1.0 speaks 1 (this project's engine: 0.2.0): "
                  "use the nnh of NNHGE 0.2.0"));
}

NNH_TEST(a_game_refusing_the_hello_is_a_protocol_refusal) {
    ScriptRunner r = runner_for("wait 1\n");
    r.set_versions("0.1.0", "0.2.0");
    r.start();
    std::uint8_t out[wire::kMaxFixedRecord];
    FakeConsole c(r);
    c.to_pc(out, wire::encode_rejected_record(0, wire::Reject::Range, out)); // the Hello's sequence number
    CHECK(r.verdict() == Verdict::Error && r.exit_code() == 2 && !r.attached());
    CHECK(c.noted("the game refused nnh's remote protocol version 1 (nnh 0.1.0; this project's engine: 0.2.0)"));
}
