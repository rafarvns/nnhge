// The script runner's live values (spec 030): the records the game sends after a catalogue request. Stage 1 only routes
// them here; stage 6 (T6.3) resolves names, follows values for expect-value and handles LiveState.

#include "remote/script_runner.hpp"

namespace nnh::tool::remote {

void ScriptRunner::on_live_record(const Record&) {}

} // namespace nnh::tool::remote
