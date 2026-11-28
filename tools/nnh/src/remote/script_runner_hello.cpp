// Script runner (see script_runner.hpp): the game's answer to nnh's Hello, or its refusal. A game that speaks another
// remote protocol version ends the run with spec 033's message (protocol_mismatch.hpp, FR-22): both versions, this
// nnh's engine version and the project's, and the fix.

#include "protocol_mismatch.hpp"
#include "remote/script_runner.hpp"

namespace nnh::tool::remote {

void ScriptRunner::hello_record(const Record& record) {
    if (!hello_sent_) return;
    attached_ = true;
    if (record.frame > last_frame_) last_frame_ = record.frame;
    if (record.version != wire::kVersion) {
        finish(Verdict::Error, 0,
               protocol_mismatch_message(ProtocolSide::Remote, record.version, wire::kVersion, nnh_version_,
                                         project_engine_));
        return;
    }
    capabilities_ = record.capabilities;
    if (options_.interactive) {
        for (const Step& step : waiting_) typed_step(step);
        waiting_.clear();
        pump_typed();
    } else {
        hello_answered(record);
    }
}

bool ScriptRunner::hello_refused(const Record& record) {
    // The Hello goes out with sequence number 0 before anything else; a game refuses a version it doesn't speak
    // (Reject::Range) before attaching.
    if (attached_ || !hello_sent_ || record.seq != 0) return false;
    finish(Verdict::Error, 0,
           protocol_mismatch_message(ProtocolSide::Remote, std::nullopt, wire::kVersion, nnh_version_, project_engine_));
    return true;
}

} // namespace nnh::tool::remote
