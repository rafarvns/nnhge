#pragma once
// A scene that records its callbacks into a shared log, for the scene tests.

#include <cstring>

#include "nnh/scene.hpp"

namespace scene_test {

/// Entries like "A+" (enter), "A-" (exit), "Au" (update), "Ad" (draw), separated by spaces.
struct Log {
    char text[512] = {};
    void add(char name, char what) {
        const std::size_t n = std::strlen(text);
        if (n + 4 >= sizeof text) return;
        if (n > 0) text[n] = ' ';
        const std::size_t at = n > 0 ? n + 1 : 0;
        text[at] = name;
        text[at + 1] = what;
        text[at + 2] = '\0';
    }
    bool is(const char* expected) const { return std::strcmp(text, expected) == 0; }
    void clear() { text[0] = '\0'; }
};

class RecordingScene : public nnh::Scene {
public:
    RecordingScene(char name, Log* log) : name_(name), log_(log) {}
    void on_enter() override { log_->add(name_, '+'); }
    void on_exit() override { log_->add(name_, '-'); }
    void update() override { log_->add(name_, 'u'); }
    void draw() override { log_->add(name_, 'd'); }

private:
    char name_;
    Log* log_;
};

} // namespace scene_test
