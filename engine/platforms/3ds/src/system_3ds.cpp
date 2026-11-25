// Nintendo 3DS platform: system description from the console model (contract nnh/platform/system.hpp).

#include <3ds.h>

#include "nnh/platform/system.hpp"

namespace nnh::platform {
namespace {

constexpr std::uint32_t bit(Capability c) { return 1u << static_cast<unsigned>(c); }

/// Console model via the CFG service, read once (system::stereo_slider() asks for the capabilities every frame with 3D
/// on, spec 026); CFG_MODEL_3DS if it can't be read.
u8 console_model() {
    static int cached = -1;
    if (cached >= 0) return static_cast<u8>(cached);
    u8 model = CFG_MODEL_3DS;
    if (R_SUCCEEDED(cfguInit())) {
        CFGU_GetSystemModel(&model);
        cfguExit();
    }
    cached = model;
    return model;
}

const char* model_name(u8 model) {
    switch (model) {
    case CFG_MODEL_3DS: return "Old 3DS";
    case CFG_MODEL_3DSXL: return "Old 3DS XL";
    case CFG_MODEL_N3DS: return "New 3DS";
    case CFG_MODEL_2DS: return "Old 2DS";
    case CFG_MODEL_N3DSXL: return "New 3DS XL";
    case CFG_MODEL_N2DSXL: return "New 2DS XL";
    default: return "3DS (unknown model)";
    }
}

bool is_new(u8 model) { return model == CFG_MODEL_N3DS || model == CFG_MODEL_N3DSXL || model == CFG_MODEL_N2DSXL; }
bool has_stereo(u8 model) { return model != CFG_MODEL_2DS && model != CFG_MODEL_N2DSXL; }

} // namespace

std::uint32_t capabilities() {
    const u8 model = console_model();
    std::uint32_t caps = bit(Capability::SecondScreen) | bit(Capability::TouchScreen) | bit(Capability::CirclePad);
    if (has_stereo(model)) caps |= bit(Capability::Stereo3D);
    if (is_new(model)) caps |= bit(Capability::CStick) | bit(Capability::ZButtons);
    return caps;
}

SystemInfo system_info() {
    const std::uint32_t bytes = envGetHeapSize() + envGetLinearHeapSize();
    return SystemInfo{"Nintendo 3DS", model_name(console_model()), bytes / (1024 * 1024)};
}

std::uint32_t poll_dev_requests() { return 0; } // the overlay combo (SELECT + L) is handled by core

} // namespace nnh::platform
