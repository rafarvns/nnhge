// apply() (spec 018 FR-13): the module's only gfx::set_camera call, on the screen(s) the rig drives.

#include "camera/camera_internal.hpp"

#include <initializer_list>

namespace nnh {

void CameraRig::apply() const {
    for (Screen screen : {Screen::Top, Screen::Bottom}) {
        if (drives(screen)) gfx::set_camera(screen, camera(screen));
    }
}

} // namespace nnh
