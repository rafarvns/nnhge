#pragma once
// The showcase's screens (spec 029 T7.3): title, options (the language list), dialog (wrapped, revealed text), shop
// (plurals, grouping, a name argument) and the font gallery. Their texts are keys resolved by loc::resolve; the scenes
// that draw by hand call loc::tr / loc::format with the role fonts.

#include <cstdint>

#include "loc_scene.hpp"

class TitleScene final : public LocScene {
public:
    explicit TitleScene(Demo& demo) : LocScene(demo, "title") {}
    bool create();

private:
    void step() override;
    bool announced_ = false;
    std::uint64_t last_frame_ = 0; ///< the frame of the last step (a gap: back from a scene above)
};

class OptionsScene final : public LocScene {
public:
    explicit OptionsScene(Demo& demo) : LocScene(demo, "options") {}
    bool create();
    void on_enter() override;

private:
    void step() override;
    void language_changed() override;
    nnh::UiWidget choice_{};
    nnh::UiText names_[nnh::loc::kMaxLanguages + 1]; ///< "System language", then each language by its own name
    int count_ = 0;
};

class DialogScene final : public LocScene {
public:
    explicit DialogScene(Demo& demo) : LocScene(demo, "dialog") {}
    bool create();
    void on_enter() override;

private:
    void step() override;
    nnh::UiWidget text_{};
    bool logged_ = false;
};

class ShopScene final : public LocScene {
public:
    explicit ShopScene(Demo& demo) : LocScene(demo, "shop") {}
    bool create();
    void on_enter() override;

private:
    void step() override;
    void paint() const override;
    void language_changed() override;
    char lines_[6][96] = {};
    std::uint32_t logged_ = 0; ///< loc::serial() of the lines last logged
};

class GalleryScene final : public LocScene {
public:
    explicit GalleryScene(Demo& demo) : LocScene(demo, "gallery") {}
    bool create();

private:
    void step() override;
    void paint() const override;
    void language_changed() override;
};
