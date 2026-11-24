#pragma once
// The showcase's choices (effect, reveal, screens, iris centre) and the transition options they make.

#include <cstdint>
#include <nnh/transition.hpp>

class Curtain;

enum class FxChoice : std::uint8_t { Fade, Wipe, Split, Blinds, Iris, Diamond, Curtain, Count };
enum class ScreensChoice : std::uint8_t { Both, TopOnly, Tall, Count };

struct Settings {
    FxChoice fx = FxChoice::Fade;
    bool continue_reveal = false;
    ScreensChoice screens = ScreensChoice::Both;
    nnh::transition::IrisCenter centre{}; ///< the middle until a touch sets a bottom-screen point
};

const char* fx_name(FxChoice fx);
const char* screens_name(ScreensChoice screens);

/// The options of a room change with these settings: out and in 24 frames, the curtain for FxChoice::Curtain.
nnh::transition::Options make_options(const Settings& settings, Curtain& curtain);
