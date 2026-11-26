// The platformer's live values (see tuning.hpp).

#include "tuning.hpp"

PlatformerTuning g_tuning;

void register_tuning(TuningHandles& h) {
    namespace live = nnh::live;
    h.run = live::tune("player/run", &g_tuning.run_speed, {.min = 0.25f, .max = 4, .step = 0.25f});
    h.gravity = live::tune("player/gravity", &g_tuning.gravity, {.min = 0.05f, .max = 1, .step = 0.01f});
    h.jump = live::tune("player/jump", &g_tuning.jump_speed, {.min = 0, .max = 12, .step = 0.1f});
    h.max_fall = live::tune("player/max_fall", &g_tuning.max_fall, {.min = 1, .max = 12, .step = 0.5f});
    h.look_ahead = live::tune("camera/look_ahead", &g_tuning.look_ahead, {.min = 0, .max = 96, .step = 4});
    h.look_ahead_speed = live::tune("camera/look_ahead_speed", &g_tuning.look_ahead_speed, {.min = 0.25f, .max = 8, .step = 0.25f});
}
