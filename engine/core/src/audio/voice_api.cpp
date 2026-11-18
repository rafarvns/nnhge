// Voices and groups (nnh::audio): play, change, stop; group and master volume and pause. Every rule is
// here, in core; the platform only receives the resulting gains, rate and pause state.

#include "audio/audio_internal.hpp"
#include "log/engine_channels.hpp"
#include "nnh/log.hpp"

namespace nnh::audio {

namespace {

using detail::audio_state;
using detail::VoiceState;

/// Seconds a voice plays at `pitch` (the clock without audio output).
double play_seconds(const detail::SoundSlot& sound, float pitch) {
    return static_cast<double>(sound.frames) / (static_cast<double>(sound.rate) * detail::clamp_pitch(pitch));
}

void apply_params(int index) {
    detail::AudioState& a = audio_state();
    if (!a.available) return;
    const VoiceState& v = a.voices.at(index);
    platform::voice_update(index, a.mixer.params(v.volume, v.pan, v.pitch, a.sounds[v.sound_slot].rate, v.group));
}

void apply_pause(int index) {
    detail::AudioState& a = audio_state();
    const VoiceState& v = a.voices.at(index);
    if (a.available) platform::voice_pause(index, a.mixer.paused(v.paused, v.group));
}

VoiceState* live(Voice voice) {
    detail::AudioState& a = audio_state();
    return a.ready ? a.voices.get(voice) : nullptr;
}

} // namespace

Voice play(const Sound& sound, const PlayOptions& options) {
    detail::AudioState& a = audio_state();
    if (!a.ready) return {};
    const int sound_slot = SoundAccess::slot(sound);
    detail::SoundSlot* s = detail::sound_get(sound_slot, SoundAccess::generation(sound));
    if (s == nullptr) return {};
    const int index = a.voices.choose(options.priority);
    if (index < 0) {
        if (!s->drop_logged) {
            NNH_LOG_WARN(audio, "a sound was dropped: all %d voices play higher priorities (AppConfig::audio.voices)",
                         a.voices.count());
            s->drop_logged = true;
        }
        return {};
    }
    if (a.voices.at(index).active) detail::end_voice(index); // replaced: no SoundFinished
    const Voice voice = a.voices.claim(index, options.priority);
    VoiceState& v = a.voices.at(index);
    v.sound_slot = sound_slot;
    v.sound_generation = SoundAccess::generation(sound);
    v.group = options.group;
    v.volume = detail::clamp_volume(options.volume);
    v.pan = detail::clamp_pan(options.pan);
    v.pitch = detail::clamp_pitch(options.pitch);
    v.loop = options.loop;
    v.silent_left = play_seconds(*s, v.pitch);
    if (a.available) {
        platform::voice_start(index, s->sample, s->frames, s->channels, v.loop,
                              a.mixer.params(v.volume, v.pan, v.pitch, s->rate, v.group));
        if (a.mixer.group_paused(v.group)) platform::voice_pause(index, true);
    }
    return voice;
}

void stop(Voice voice) {
    if (live(voice) != nullptr) detail::end_voice(voice.index);
}

void set_paused(Voice voice, bool paused) {
    if (VoiceState* v = live(voice)) {
        v->paused = paused;
        apply_pause(voice.index);
    }
}

void set_volume(Voice voice, float volume) {
    if (VoiceState* v = live(voice)) {
        v->volume = detail::clamp_volume(volume);
        apply_params(voice.index);
    }
}

void set_pan(Voice voice, float pan) {
    if (VoiceState* v = live(voice)) {
        v->pan = detail::clamp_pan(pan);
        apply_params(voice.index);
    }
}

void set_pitch(Voice voice, float pitch) {
    if (VoiceState* v = live(voice)) {
        const float old = v->pitch;
        v->pitch = detail::clamp_pitch(pitch);
        v->silent_left *= static_cast<double>(old) / static_cast<double>(v->pitch); // the rest plays faster/slower
        apply_params(voice.index);
    }
}

bool playing(Voice voice) { return live(voice) != nullptr; }

void set_volume(AudioGroup group, float volume) {
    audio_state().mixer.set_group_volume(group, volume);
    detail::for_each_voice([group](int i) {
        if (audio_state().voices.at(i).group == group) apply_params(i);
    });
    if (audio_state().music != nullptr) audio_state().music->mix_changed();
}

float volume(AudioGroup group) { return audio_state().mixer.group_volume(group); }

void set_master_volume(float volume) {
    audio_state().mixer.set_master(volume);
    detail::for_each_voice(apply_params);
    if (audio_state().music != nullptr) audio_state().music->mix_changed();
}

float master_volume() { return audio_state().mixer.master(); }

void set_paused(AudioGroup group, bool paused) {
    audio_state().mixer.set_group_paused(group, paused);
    detail::for_each_voice([group](int i) {
        if (audio_state().voices.at(i).group == group) apply_pause(i);
    });
    if (audio_state().music != nullptr) audio_state().music->mix_changed();
}

bool paused(AudioGroup group) { return audio_state().mixer.group_paused(group); }

bool available() { return audio_state().available; }

} // namespace nnh::audio
