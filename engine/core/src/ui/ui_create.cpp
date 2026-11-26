// Creating a UI (spec 028 FR-1, FR-3): description checks, one block of tagged memory (header and tables), the roots,
// moves, destruction, stats, misuse reporting.

#include <new>

#include "nnh/assert.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "ui/ui_block.hpp"
#include "ui/ui_desc_check.hpp"

NNH_MEM_TAG(ui);
NNH_LOG_CHANNEL(ui);

namespace nnh {
namespace detail {

MemTag& ui_memory_tag() { return NNH_MEM_TAG_REF(ui); }

void ui_misuse(UiBlock& b, const char* what) {
    if (what != nullptr) {
        NNH_LOG_ERROR(ui, "%s", what);
        NNH_ASSERT(false, "%s", what);
        return;
    }
    if (!b.error_logged) { // Release: one line per UI, no misuse text in the binary
        b.error_logged = true;
        NNH_LOG_ERROR(ui, "a UI call was refused (a Debug or Development build names it)");
    }
}

void ui_unbind(UiBlock& b); // ui_scene.cpp

void ui_free(UiBlock& b) {
    ui_unbind(b);
    b.~UiBlock();
    mem::free(&b);
}

namespace {

constexpr std::uint32_t align16(std::uint32_t v) { return (v + 15u) & ~15u; }

struct Offsets {
    std::uint32_t hot, cold, text, texts, text_kinds, styles, rows, fns, fn_used, results, total;
};

Offsets offsets(const UiDesc& d) {
    Offsets o{};
    std::uint32_t at = align16(sizeof(UiBlock));
    auto take = [&](std::uint32_t bytes) {
        const std::uint32_t here = at;
        at = align16(at + bytes);
        return here;
    };
    const auto n = [](int v) { return static_cast<std::uint32_t>(v); };
    o.hot = take(n(d.widgets) * sizeof(UiHot));
    o.cold = take(n(d.widgets) * sizeof(UiCold));
    o.text = take(n(d.text_bytes));
    o.texts = take(n(d.texts) * sizeof(UiTextRef));
    o.text_kinds = take(n(d.texts));
    o.styles = take(n(d.styles) * sizeof(UiStyle));
    o.rows = take(n(d.rows) * sizeof(UiRowSlot));
    o.fns = take(n(d.functions) * sizeof(UiFnSlot));
    o.fn_used = take(n(d.functions));
    o.results = take(n(d.results) * sizeof(UiResult));
    o.total = at;
    return o;
}

std::uint16_t g_next_serial = 0;

} // namespace

/// `count` value-initialised T at `at` (one placement new each: no array cookie).
template <typename T>
T* construct(unsigned char* at, int count) {
    T* first = reinterpret_cast<T*>(at);
    for (int i = 0; i < count; ++i) ::new (static_cast<void*>(first + i)) T{};
    return first;
}

namespace {

void make_root(UiBlock& b, Screen screen) {
    const int s = tree_take(b, UiKind::Root, -1);
    UiHot& n = b.hot[s];
    n.flags |= screen == Screen::Bottom ? kUiBottom : 0;
    n.w = static_cast<std::int16_t>(screen_width(screen));
    n.h = static_cast<std::int16_t>(screen_height(screen));
    b.cold[s].data.box = UiBoxData{};
    b.roots[screen_index(screen)] = static_cast<std::uint16_t>(s);
}

} // namespace

std::uint32_t ui_block_bytes(const UiDesc& desc) { return offsets(desc).total; }

} // namespace detail

Result<Ui> Ui::create(const UiDesc& desc) {
    const Error e = detail::check_ui_desc(desc);
    if (e.code != ErrorCode::None) return e;
    const detail::Offsets o = detail::offsets(desc);
    MemTag& tag = desc.tag != nullptr ? *desc.tag : detail::ui_memory_tag();
    auto* raw = static_cast<unsigned char*>(mem::alloc(MemRegion::Heap, tag, o.total, 16));
    if (raw == nullptr) {
        return Error::make(ErrorCode::OutOfMemory, "ui: no memory for %d widgets (%u bytes)", desc.widgets,
                           static_cast<unsigned>(o.total));
    }
    auto* b = ::new (raw) detail::UiBlock{};
    b->hot = detail::construct<detail::UiHot>(raw + o.hot, desc.widgets);
    b->cold = detail::construct<detail::UiCold>(raw + o.cold, desc.widgets);
    b->text = reinterpret_cast<char*>(raw + o.text);
    b->texts = detail::construct<detail::UiTextRef>(raw + o.texts, desc.texts);
    b->text_kinds = detail::construct<std::uint8_t>(raw + o.text_kinds, desc.texts);
    b->styles = detail::construct<UiStyle>(raw + o.styles, desc.styles);
    b->rows = detail::construct<detail::UiRowSlot>(raw + o.rows, desc.rows);
    b->fns = reinterpret_cast<detail::UiFnSlot*>(raw + o.fns);
    b->fn_used = detail::construct<std::uint8_t>(raw + o.fn_used, desc.functions);
    b->results = detail::construct<UiResult>(raw + o.results, desc.results);
    b->capacity = desc.widgets;
    b->text_capacity = desc.text_bytes;
    b->texts_capacity = desc.texts;
    b->styles_capacity = desc.styles;
    b->rows_capacity = desc.rows;
    b->fns_capacity = desc.functions;
    b->results_capacity = desc.results;
    b->bytes = o.total;
    b->tag = &tag;
    b->serial = ++detail::g_next_serial;
    b->first_focus = desc.first_focus;
    b->input = desc.input;
    b->theme = &ui::default_theme();
    b->root_visible[0] = desc.top;
    b->root_visible[1] = desc.bottom;
    if (desc.top) detail::make_root(*b, Screen::Top);
    if (desc.bottom) detail::make_root(*b, Screen::Bottom);
    Ui ui;
    ui.block_ = b;
    return ui;
}

Ui::Ui(Ui&& other) noexcept : block_(other.block_) {
    if (block_ != nullptr && block_->busy > 0) NNH_UI_MISUSE(*block_, "Ui moved from its own callback");
    other.block_ = nullptr;
}

Ui& Ui::operator=(Ui&& other) noexcept {
    if (this != &other) {
        Ui gone(static_cast<Ui&&>(*this));
        block_ = other.block_;
        other.block_ = nullptr;
    }
    return *this;
}

Ui::~Ui() {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    block_ = nullptr;
    if (b->busy > 0) {
        NNH_UI_MISUSE(*b, "Ui destroyed from its own callback");
        b->orphaned = true; // Release: freed when the running update ends
        return;
    }
    detail::ui_free(*b);
}

bool Ui::valid() const { return block_ != nullptr; }

UiWidget Ui::root(Screen screen) const {
    if (block_ == nullptr) return {};
    return detail::handle_of(*block_, block_->roots[detail::screen_index(screen)] == detail::kUiNone
                                          ? -1
                                          : block_->roots[detail::screen_index(screen)]);
}

UiStats Ui::stats() const {
    UiStats s;
    const detail::UiBlock* b = block_;
    if (b == nullptr) return s;
    s.widgets = b->live;
    s.capacity = b->capacity;
    s.text_bytes = b->text_used;
    s.text_capacity = b->text_capacity;
    s.rows = b->rows_used;
    s.functions = b->fns_used;
    s.layout_visits = b->layout_visits;
    s.layouts = b->layouts;
    s.draws = b->draws;
    s.draw_hash = b->draw_hash;
    s.dropped_results = b->dropped_results;
    s.block_bytes = b->bytes;
    return s;
}

} // namespace nnh
