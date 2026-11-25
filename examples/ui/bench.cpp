#include "bench.hpp"

namespace {

int g_fills = 0;

const char* bench_text(std::uint32_t key) {
    static const char* const kTexts[4] = {"Continue", "Options", "Inventory", "Quit game"};
    return kTexts[key % 4];
}

void fill(int index, nnh::UiRow& row) {
    ++g_fills;
    row.text = "Potion of the north";
    row.number = index % 99;
    row.number_prefix = "x";
}

nnh::UiInput press(std::uint8_t c) {
    nnh::UiInput in;
    in.held = in.pressed = c;
    return in;
}

} // namespace

bool UiBench::init() {
    auto big = nnh::Ui::create({.widgets = 128, .text_bytes = 256});
    auto a = nnh::Ui::create({.widgets = 4, .rows = 12, .functions = 1});
    auto b = nnh::Ui::create({.widgets = 4, .rows = 12, .functions = 1});
    if (!big || !a || !b) return false;
    big_ = static_cast<nnh::Ui&&>(big.value());
    list_a_ = static_cast<nnh::Ui&&>(a.value());
    list_b_ = static_cast<nnh::Ui&&>(b.value());
    big_.set_resolver(&bench_text);
    // the bottom root: two columns of 20 buttons (64 keyed texts with the top's); the top: 84 labels and boxes
    for (int c = 0; c < 2; ++c) {
        column_[c] = big_.vbox(big_.root(nnh::Screen::Bottom),
                               {.spacing = 1, .layout = {.width = 100, .place = {.x = 20 + c * 160, .y = 4}}});
        for (int i = 0; i < 20; ++i) big_.button(column_[c], {.text = nnh::UiText::key(static_cast<std::uint32_t>(i))});
    }
    const nnh::UiWidget grid = big_.grid_box(big_.root(nnh::Screen::Top), {.spacing = 2, .columns = 4});
    for (int i = 0; i < 24; ++i) big_.label(grid, {.text = nnh::UiText::key(static_cast<std::uint32_t>(i))});
    for (int i = 0; i < 11; ++i) {
        const nnh::UiWidget row = big_.hbox(grid);
        for (int k = 0; k < 4; ++k) big_.label(row, {.text = "ab"});
    }
    big_.update(nnh::UiInput{});
    long_ = list_a_.list(list_a_.root(nnh::Screen::Bottom), {.rows = 12, .count = 10000, .fill = &fill});
    short_ = list_b_.list(list_b_.root(nnh::Screen::Bottom), {.rows = 12, .count = 20, .fill = &fill});
    list_a_.set_item(long_, 5000);
    list_a_.update(nnh::UiInput{});
    list_b_.update(nnh::UiInput{});
    return big_.stats().widgets <= 128;
}

void UiBench::idle128() { big_.update(nnh::UiInput{}); }

void UiBench::move128(int frame) {
    big_.update(press(nnh::ui_bit(frame % 2 == 0 ? nnh::UiControl::Down : nnh::UiControl::Up)));
}

void UiBench::geo128(int frame) {
    big_.update(press(nnh::ui_bit(frame % 2 == 0 ? nnh::UiControl::Right : nnh::UiControl::Left)));
}

void UiBench::relayout128() {
    big_.text_changed();
    big_.update(nnh::UiInput{});
}

void UiBench::list10000(int frame) {
    list_a_.update(press(nnh::ui_bit(frame % 16 < 8 ? nnh::UiControl::Down : nnh::UiControl::Up)));
}

void UiBench::list20(int frame) {
    list_b_.update(press(nnh::ui_bit(frame % 16 < 8 ? nnh::UiControl::Down : nnh::UiControl::Up)));
}

void UiBench::draw_list(bool long_list) const { (long_list ? list_a_ : list_b_).draw(); }

int UiBench::list_fills() const { return g_fills; }
