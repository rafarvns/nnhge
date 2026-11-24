// The saves example's tiny game (tiny_game.hpp).

#include "tiny_game.hpp"

#include <new>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace saves_example {
namespace {

constexpr float kTile = 20;
constexpr float kRoomW = 400;
constexpr float kRoomH = 240;
constexpr float kSize = 12; // the player's side
constexpr const char* kNames[] = {"Ana", "Zo\xC3\xAB", "Bo", "Kai of the Long Road"};
constexpr int kNameCount = static_cast<int>(sizeof kNames / sizeof kNames[0]);

/// A spot in the room from its index (the coin's and the item's places).
nnh::Vec2 spot(int room, int salt) {
    const std::uint32_t h =
        (static_cast<std::uint32_t>(room) * 2654435761u) ^ (static_cast<std::uint32_t>(salt) << 20);
    return {40 + static_cast<float>(h % 300), 40 + static_cast<float>((h >> 12) % 150)};
}

bool has_item(int room) {
    return room % 5 == 2;
}

bool touches(nnh::Vec2 player, nnh::Vec2 thing) {
    return player.x < thing.x + 10 && thing.x < player.x + kSize && player.y < thing.y + 10 &&
           thing.y < player.y + kSize;
}

} // namespace

void TinyGame::reset() {
    ::new (&p_) Progress{}; // in place: no 5 KB temporary on the stack
    name_index_ = 0;
    p_.name.assign(kNames[0]);
    p_.position = {kRoomW / 2, kRoomH / 2};
    enter_room(0);
}

const char* TinyGame::rename() {
    name_index_ = (name_index_ + 1) % kNameCount;
    p_.name.assign(kNames[name_index_]);
    return kNames[name_index_];
}

void TinyGame::enter_room(int room) {
    p_.room = static_cast<std::uint16_t>(room);
    coin_taken_ = false;
    item_taken_ = false;
    p_.explored[room] = true;
    int explored = 0;
    for (bool e : p_.explored)
        explored += e ? 1 : 0;
    p_.level = static_cast<std::uint16_t>(1 + explored / 4);
}

void TinyGame::collect() {
    if (!coin_taken_ && touches(p_.position, spot(p_.room, 1))) {
        coin_taken_ = true;
        ++p_.gems;
    }
    if (has_item(p_.room) && !item_taken_ && touches(p_.position, spot(p_.room, 2)) &&
        p_.inventory.size() < 64) {
        item_taken_ = true;
        Item it;
        it.kind = static_cast<std::uint16_t>(200 + p_.room % 50);
        it.slot = static_cast<std::uint8_t>(p_.inventory.size() % 8);
        it.label.assign("found");
        p_.inventory.push_back(it);
    }
}

void TinyGame::update() {
    const nnh::Stick pad = nnh::input::circle_pad();
    float dx = pad.x, dy = -pad.y;
    if (nnh::input::held(nnh::Button::Left)) dx = -1;
    if (nnh::input::held(nnh::Button::Right)) dx = 1;
    if (nnh::input::held(nnh::Button::Up)) dy = -1;
    if (nnh::input::held(nnh::Button::Down)) dy = 1;
    if (dx == 0 && dy == 0) return;
    ++p_.play_frames;
    p_.position.x += dx * kSpeed;
    p_.position.y += dy * kSpeed;
    int rx = p_.room % kWorldSide, ry = p_.room / kWorldSide;
    bool moved = false;
    if (p_.position.x < 0) {
        p_.position.x += kRoomW - kSize;
        rx = (rx + kWorldSide - 1) % kWorldSide;
        moved = true;
    }
    if (p_.position.x > kRoomW - kSize) {
        p_.position.x -= kRoomW - kSize;
        rx = (rx + 1) % kWorldSide;
        moved = true;
    }
    if (p_.position.y < 0) {
        p_.position.y += kRoomH - kSize;
        ry = (ry + kWorldSide - 1) % kWorldSide;
        moved = true;
    }
    if (p_.position.y > kRoomH - kSize) {
        p_.position.y -= kRoomH - kSize;
        ry = (ry + 1) % kWorldSide;
        moved = true;
    }
    if (moved) enter_room(ry * kWorldSide + rx);
    p_.tile = static_cast<std::uint16_t>(static_cast<int>(p_.position.y / kTile) * 20 +
                                         static_cast<int>(p_.position.x / kTile));
    collect();
}

void TinyGame::draw() const {
    namespace gfx = nnh::gfx;
    const nnh::Screen top = nnh::Screen::Top;
    for (int x = 0; x <= 20; ++x)
        gfx::fill_rect(top, {x * kTile, 0, 1, kRoomH}, nnh::rgb(24, 30, 48));
    for (int y = 0; y <= 12; ++y)
        gfx::fill_rect(top, {0, y * kTile, kRoomW, 1}, nnh::rgb(24, 30, 48));
    if (!coin_taken_) {
        const nnh::Vec2 c = spot(p_.room, 1);
        gfx::fill_rect(top, {c.x, c.y, 10, 10}, nnh::colors::yellow);
    }
    if (has_item(p_.room) && !item_taken_) {
        const nnh::Vec2 i = spot(p_.room, 2);
        gfx::fill_rect(top, {i.x, i.y, 10, 10}, nnh::rgb(80, 220, 230));
    }
    gfx::fill_rect(top, {p_.position.x, p_.position.y, kSize, kSize}, nnh::colors::white);
}

} // namespace saves_example
