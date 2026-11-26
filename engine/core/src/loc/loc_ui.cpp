// loc::apply (nnh/localization.hpp; spec 029 FR-30): spec 028's widgets in the current language — the only localization
// file that includes <nnh/ui.hpp>, so the UI never depends on localization.

#include "nnh/localization.hpp"
#include "nnh/ui.hpp"

namespace nnh {

void loc::apply(Ui& ui, const UiFontRole* roles, int count) {
    ui.set_resolver(&loc::resolve);
    for (int i = 0; roles != nullptr && i < count; ++i) ui.set_font(roles[i].slot, font(roles[i].role));
    ui.text_changed();
}

} // namespace nnh
