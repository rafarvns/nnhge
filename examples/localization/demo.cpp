#include "demo.hpp"

#include <cstdlib>
#include <cstring>

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("LOC_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

bool shot_is(const char* name) {
    const char* shot = std::getenv("LOC_SHOT");
    return shot != nullptr && std::strcmp(shot, name) == 0;
}

const nnh::UiTheme& example_theme() {
    static nnh::UiTheme theme = [] {
        nnh::UiTheme t = nnh::ui::default_theme();
        nnh::UiStyle* styles[] = {&t.label, &t.button, &t.toggle, &t.slider, &t.choice, &t.list, &t.grid, &t.panel};
        for (nnh::UiStyle* s : styles) s->text.font = nnh::UiFont::Slot0;
        return t;
    }();
    return theme;
}

nnh::UiStyle title_style() {
    nnh::UiStyle s = example_theme().label;
    s.text.font = nnh::UiFont::Slot1;
    for (nnh::UiLook& look : s.states) look.text = nnh::rgb(255, 214, 120);
    s.text.shadow = true;
    return s;
}

void next_language() {
    const int count = nnh::loc::language_count();
    int next = nnh::loc::index();
    for (int i = 0; i < count; ++i) {
        next = (next + 1) % count;
        if (std::strcmp(nnh::loc::language_code(next), "qps") != 0) break;
    }
    (void)nnh::loc::set_language(nnh::loc::language_code(next));
}
