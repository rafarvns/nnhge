// The live panel's model (see live_panel_model.hpp).

#include "live/live_panel_model.hpp"

#include <cstring>

#include "debug/dev_controls.hpp"
#include "live/live_apply.hpp"
#include "live/live_names.hpp"
#include "live/live_steps.hpp"
#include "nnh/input.hpp"

namespace nnh::detail::live {
namespace {

constexpr std::uint32_t bit(Button b) { return static_cast<std::uint32_t>(b); }

struct EntryName {
    const char* name = nullptr;
    int length = 0;
    RowType type = RowType::Tunable;
    std::uint16_t ref = 0;
};

int entry_count(const Registry& r, int page) {
    return page == kPageTunables ? r.tunable_count : r.watch_count + r.shown_count;
}

EntryName entry_at(const Registry& r, int page, int i) {
    if (page == kPageTunables) {
        const Entry& e = r.entries[r.tunable_index[i]];
        return {e.name, e.name_length, RowType::Tunable, r.tunable_index[i]};
    }
    const std::uint16_t ref = r.watch_index[i];
    if ((ref & kShownRef) != 0) {
        const ShownEntry& s = r.shown[ref & ~kShownRef];
        return {s.name, s.name_length, RowType::Shown, static_cast<std::uint16_t>(ref & ~kShownRef)};
    }
    const WatchEntry& w = r.watches[ref];
    return {w.name, w.name_length, RowType::Watch, ref};
}

bool same_group(const char* a, int an, const char* b, int bn, int level) {
    const std::size_t ga = group_length(a, static_cast<std::size_t>(an), level);
    const std::size_t gb = group_length(b, static_cast<std::size_t>(bn), level);
    return ga != 0 && ga == gb && std::memcmp(a, b, ga) == 0;
}

/// Entries from index `from` on inside the group path `name[0, length)`.
int count_in_group(const Registry& r, int page, int from, const char* name, int length) {
    int n = 0;
    for (int i = from; i < entry_count(r, page); ++i) {
        const EntryName e = entry_at(r, page, i);
        if (!in_group(e.name, static_cast<std::size_t>(e.length), name, static_cast<std::size_t>(length))) break;
        ++n;
    }
    return n;
}

/// Calls fn(row) for every visible row in order until it returns false.
template <typename Fn>
void each_row(const Registry& r, int page, Fn fn) {
    const EntryName none;
    EntryName prev = none;
    for (int i = 0; i < entry_count(r, page); ++i) {
        const EntryName e = entry_at(r, page, i);
        const int depth = group_depth(e.name, static_cast<std::size_t>(e.length));
        int common = 0;
        while (common < depth && prev.name != nullptr && same_group(prev.name, prev.length, e.name, e.length, common)) {
            ++common;
        }
        bool visible = true;
        for (int level = 0; level < depth && visible; ++level) {
            const int gl = static_cast<int>(group_length(e.name, static_cast<std::size_t>(e.length), level));
            const bool open = group_open(r.panel, fnv1a32(e.name, static_cast<std::size_t>(gl)));
            if (level >= common) {
                Row row;
                row.type = RowType::Group;
                row.depth = static_cast<std::uint8_t>(level);
                row.length = static_cast<std::uint8_t>(gl);
                row.open = open;
                row.count = static_cast<std::uint16_t>(count_in_group(r, page, i, e.name, gl));
                row.name = e.name;
                if (!fn(row)) return;
            }
            visible = open;
        }
        if (visible) {
            Row row;
            row.type = e.type;
            row.depth = static_cast<std::uint8_t>(depth);
            row.length = static_cast<std::uint8_t>(e.length);
            row.ref = e.ref;
            row.name = e.name;
            if (!fn(row)) return;
        }
        prev = e;
    }
}

int& selected_of(Registry& r, int page) { return page == kPageTunables ? r.panel.selected : r.panel.watch_selected; }
int& scroll_of(Registry& r, int page) { return page == kPageTunables ? r.panel.scroll : r.panel.watch_scroll; }
int visible_rows(int page) { return page == kPageTunables ? kTunableRowsVisible : kWatchRowsVisible; }

void keep_visible(Registry& r, int page) {
    int& sel = selected_of(r, page);
    int& scroll = scroll_of(r, page);
    const int count = row_count(r, page);
    if (sel >= count) sel = count > 0 ? count - 1 : 0;
    if (sel < 0) sel = 0;
    if (sel < scroll) scroll = sel;
    if (sel >= scroll + visible_rows(page)) scroll = sel - visible_rows(page) + 1;
    if (scroll < 0) scroll = 0;
}

std::uint32_t group_hash_of(const Row& row) { return fnv1a32(row.name, row.length); }

/// The index of the group row whose path is `name[0, length)`, or -1.
int find_group_row(const Registry& r, int page, const char* name, int length) {
    int index = -1, at = 0;
    each_row(r, page, [&](const Row& row) {
        if (row.type == RowType::Group && row.length == length && std::memcmp(row.name, name, static_cast<std::size_t>(length)) == 0) {
            index = at;
            return false;
        }
        ++at;
        return true;
    });
    return index;
}

void reset_row(Registry& r, const Row& row) {
    if (row.type == RowType::Tunable) {
        const Entry& e = r.entries[row.ref];
        if (e.kind != Kind::Action) set_pending(r, row.ref, e.def);
        return;
    }
    for (int i = 0; i < r.tunable_count; ++i) {
        const Entry& e = r.entries[r.tunable_index[i]];
        if (e.kind != Kind::Action && in_group(e.name, e.name_length, row.name, row.length)) {
            set_pending(r, r.tunable_index[i], e.def);
        }
    }
}

void press_a(Registry& r, int page, const Row& row) {
    if (row.type == RowType::Group) {
        toggle_group(r.panel, group_hash_of(row));
        return;
    }
    if (page != kPageTunables) return;
    Entry& e = r.entries[row.ref];
    if (e.kind == Kind::Bool) set_pending(r, row.ref, edit_base(e) != 0 ? 0u : 1u);
    else if (e.kind == Kind::Color) r.panel.channel = (r.panel.channel + 1) % 4;
    else if (e.kind == Kind::Action) request_run(r, row.ref);
}

void press_b(Registry& r, int page, const Row& row) {
    const int level = row.type == RowType::Group ? row.depth - 1 : row.depth - 1;
    if (level < 0) return;
    const int gl = static_cast<int>(group_length(row.name, row.type == RowType::Group ? row.length : row.length, level));
    if (gl == 0) return;
    const std::uint32_t h = fnv1a32(row.name, static_cast<std::size_t>(gl));
    if (group_open(r.panel, h)) toggle_group(r.panel, h);
    const int index = find_group_row(r, page, row.name, gl);
    if (index >= 0) selected_of(r, page) = index;
}

void move(Registry& r, int page, const DevInput& in, int count) {
    int& sel = selected_of(r, page);
    const int before = sel;
    if ((in.repeated & bit(Button::Up)) != 0) {
        if (sel > 0) --sel;
        else if ((in.pressed & bit(Button::Up)) != 0) sel = count - 1; // wrap only on a fresh press
    }
    if ((in.repeated & bit(Button::Down)) != 0) {
        if (sel < count - 1) ++sel;
        else if ((in.pressed & bit(Button::Down)) != 0) sel = 0;
    }
    if (sel != before) r.panel.channel = 0;
}

} // namespace

bool group_open(const PanelState& p, std::uint32_t hash) {
    for (int i = 0; i < p.open_count; ++i) {
        if (p.open_groups[i] == hash) return true;
    }
    return false;
}

void toggle_group(PanelState& p, std::uint32_t hash) {
    for (int i = 0; i < p.open_count; ++i) {
        if (p.open_groups[i] != hash) continue;
        p.open_groups[i] = p.open_groups[--p.open_count];
        return;
    }
    if (p.open_count < static_cast<int>(sizeof p.open_groups / sizeof p.open_groups[0])) p.open_groups[p.open_count++] = hash;
}

int row_count(const Registry& r, int page) {
    int n = 0;
    each_row(r, page, [&](const Row&) {
        ++n;
        return true;
    });
    return n;
}

bool row_at(const Registry& r, int page, int index, Row& out) {
    int at = 0;
    bool found = false;
    each_row(r, page, [&](const Row& row) {
        if (at++ != index) return true;
        out = row;
        found = true;
        return false;
    });
    return found;
}

const char* row_label(const Row& row, int& length) {
    const char* start = leaf(row.name, row.length);
    length = static_cast<int>(row.name + row.length - start);
    return start;
}

std::uint32_t edit_base(const Entry& e) { return (e.flags & kPending) != 0 ? e.pending : read_bits(e); }

std::uint32_t stepped(const Entry& e, std::uint32_t bits, int delta, int ch) {
    switch (e.kind) {
    case Kind::Float: {
        FloatGrid g;
        if (make_float_grid(bits_float(e.min), bits_float(e.max), bits_float(e.step), g) != RangeError::None) return bits;
        return float_bits(step_value(g, bits_float(bits), delta));
    }
    case Kind::Int32: {
        IntGrid g;
        if (make_int_grid(static_cast<std::int32_t>(e.min), static_cast<std::int32_t>(e.max),
                          static_cast<std::int32_t>(e.step), g) != RangeError::None) {
            return bits;
        }
        return static_cast<std::uint32_t>(step_value(g, static_cast<std::int32_t>(bits), delta));
    }
    case Kind::Bool: return bits != 0 ? 0u : 1u;
    case Kind::Color: return with_channel(bits, ch, step_channel(channel(bits, ch), delta));
    case Kind::Enum: return step_enum(bits, e.enum_count, delta);
    default: return bits;
    }
}

int panel_selected(const Registry& r, int page) { return page == kPageTunables ? r.panel.selected : r.panel.watch_selected; }
int panel_scroll(const Registry& r, int page) { return page == kPageTunables ? r.panel.scroll : r.panel.watch_scroll; }

void panel_buttons(Registry& r, int page, const DevInput& in) {
    if ((in.held & bit(Button::Select)) != 0) return; // chords belong to the overlay's pages
    const int count = row_count(r, page);
    if (count == 0) return;
    keep_visible(r, page);
    move(r, page, in, count);
    keep_visible(r, page);
    Row row;
    if (!row_at(r, page, selected_of(r, page), row)) return;
    if ((in.pressed & bit(Button::A)) != 0) press_a(r, page, row);
    if ((in.pressed & bit(Button::B)) != 0) press_b(r, page, row);
    if (page == kPageTunables) {
        if ((in.pressed & bit(Button::X)) != 0) reset_row(r, row);
        if ((in.pressed & bit(Button::Y)) != 0) r.dump_requested = true;
        const int sign = (in.repeated & bit(Button::Right)) != 0 ? 1 : ((in.repeated & bit(Button::Left)) != 0 ? -1 : 0);
        if (sign != 0 && row.type == RowType::Tunable && r.entries[row.ref].kind != Kind::Action) {
            const int size = (in.held & (bit(Button::L) | bit(Button::R))) != 0 ? 10 : 1;
            Entry& e = r.entries[row.ref];
            set_pending(r, row.ref, stepped(e, edit_base(e), sign * size, r.panel.channel));
        }
    }
    keep_visible(r, page);
}

void panel_select_name(Registry& r, int page, const char* name) {
    if (name == nullptr) return;
    const int length = static_cast<int>(std::strlen(name));
    const int depth = group_depth(name, static_cast<std::size_t>(length));
    for (int level = 0; level < depth; ++level) {
        const std::size_t gl = group_length(name, static_cast<std::size_t>(length), level);
        const std::uint32_t h = fnv1a32(name, gl);
        if (!group_open(r.panel, h)) toggle_group(r.panel, h);
    }
    int at = 0;
    each_row(r, page, [&](const Row& row) {
        if (row.type != RowType::Group && row.length == length && std::memcmp(row.name, name, static_cast<std::size_t>(length)) == 0) {
            selected_of(r, page) = at;
            return false;
        }
        ++at;
        return true;
    });
    keep_visible(r, page);
}

} // namespace nnh::detail::live
