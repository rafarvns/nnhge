// 3DS extension <nnh/3ds/console.hpp>.

#include <3ds.h>

#include "nnh/3ds/console.hpp"

namespace nnh::n3ds {

bool is_new_3ds() {
    bool is_new = false;
    APT_CheckNew3DS(&is_new);
    return is_new;
}

float slider_3d() { return osGet3DSliderState(); }

} // namespace nnh::n3ds
