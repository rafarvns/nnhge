#pragma once
// The bestiary: every enemy the game's data assets describe (found with nnh::data::count/name, no list in code),
// the selected one's sprite and stats on the top screen, its drops on the bottom one by following references.
// UP/DOWN select · A plays its hurt sound · SELECT the self-check · START quits.
// Everything is loaded in on_enter (load time); update and draw only read.

#include <nnh/audio.hpp>
#include <nnh/scene.hpp>
#include <nnh/texture.hpp>

#include "data_types.hpp"

class BestiaryScene : public nnh::Scene {
public:
    explicit BestiaryScene(nnh::Scene& check) : check_(check) {}
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    static constexpr int kMax = 8; ///< enemies and items shown (the example has 4 and 3)

    void load_all();
    int item_index(nnh::data::Ref<Item> ref) const;
    void draw_enemy() const;
    void draw_list() const;

    nnh::Scene& check_;
    bool loaded_ = false;
    Settings settings_;
    Enemy enemies_[kMax];
    nnh::Texture sprites_[kMax];
    nnh::Sound hurt_[kMax];
    int enemy_count_ = 0;
    Item items_[kMax];
    nnh::data::Ref<Item> item_refs_[kMax];
    nnh::Texture icons_[kMax];
    int item_count_ = 0;
    int selected_ = 0;
};
