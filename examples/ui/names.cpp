#include "names.hpp"

#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

namespace {

const char* const kNames[static_cast<int>(Key::Count)] = {
    "title.new", "title.continue", "title.options", "title.inventory", "title.gallery", "title.quit", "title.extras",
    "music", "sound", "invert", "difficulty", "speed", "language", "online", "vibration", "subtitles", "hints",
    "autosave", "clock", "colours", "options.back", "list", "grid", "quit.yes", "quit.no", "dialog"};

bool indexed(nnh::UiKey k) { return k == nnh::UiKey(Key::List) || k == nnh::UiKey(Key::Grid); }

} // namespace

const char* key_name(nnh::UiKey key) {
    return key.value < static_cast<int>(Key::Count) ? kNames[key.value] : "?";
}

void log_results(const nnh::Ui& ui, const char* screen) {
    for (const nnh::UiResult& r : ui.results()) {
        const char* name = key_name(r.key);
        switch (r.kind) {
        case nnh::UiResultKind::FocusChanged:
            if (r.key.none()) break;
            if (indexed(r.key)) {
                NNH_LOG_INFO(example, "ui: focus %s %d", name, r.index);
            } else {
                NNH_LOG_INFO(example, "ui: focus %s", name);
            }
            break;
        case nnh::UiResultKind::Activated:
            if (indexed(r.key)) {
                NNH_LOG_INFO(example, "ui: activated %s %d", name, r.index);
            } else {
                NNH_LOG_INFO(example, "ui: activated %s", name);
            }
            break;
        case nnh::UiResultKind::ValueChanged: NNH_LOG_INFO(example, "ui: value %s %d", name, r.value); break;
        case nnh::UiResultKind::Refused: NNH_LOG_INFO(example, "ui: refused %s", name); break;
        case nnh::UiResultKind::Blocked: NNH_LOG_INFO(example, "ui: blocked"); break;
        case nnh::UiResultKind::Cancelled: NNH_LOG_INFO(example, "ui: cancelled %s", screen); break;
        case nnh::UiResultKind::Page: NNH_LOG_INFO(example, "ui: page %s %d", name, r.index); break;
        }
    }
}
