// Audio output (contract nnh/platform/audio.hpp, spec 008). Stage 2 stub: no output yet, so core runs
// silently (audio::available() is false); the real backend comes with spec 008 stage 4.

#include "context_3ds.hpp"
#include "nnh/platform/audio.hpp"

namespace nnh::platform {

bool audio_init(int) {
    ctr::set_error("audio output not implemented yet (spec 008 stage 4)");
    return false;
}

void audio_shutdown() {}
bool sample_create(std::uint32_t, int, SampleInfo& out) {
    out = {};
    return false;
}
void sample_ready(SampleHandle) {}
void sample_destroy(SampleHandle) {}
void voice_start(int, SampleHandle, std::uint32_t, int, bool, const VoiceParams&) {}
void voice_update(int, const VoiceParams&) {}
void voice_pause(int, bool) {}
void voice_stop(int) {}
bool voice_playing(int) { return false; }

} // namespace nnh::platform
