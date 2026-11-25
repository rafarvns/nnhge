// The widget tree (spec 028 FR-2, FR-4): lowest-free slots with generations, links (a node's first child's `prev` is
// the last child, as D37), subtree destruction (resources released, references to the destroyed slots cleared), keys,
// pre-order walks without recursion.

#include "nnh/detail/slot_bits.hpp"
#include "ui/ui_block.hpp"
#include "ui/ui_focus.hpp"

namespace nnh::detail {

void release_node(UiBlock& b, int s); // ui_release.cpp

namespace {

void unlink(UiBlock& b, int s) {
    UiHot& n = b.hot[s];
    if (n.parent == kUiNone) return;
    UiHot& p = b.hot[n.parent];
    const int first = p.first_child;
    const int last = b.hot[first].prev;
    if (first == s) {
        p.first_child = n.next;
        if (n.next != kUiNone) b.hot[n.next].prev = static_cast<std::uint16_t>(last);
    } else {
        b.hot[n.prev].next = n.next;
        if (n.next != kUiNone) {
            b.hot[n.next].prev = n.prev;
        } else {
            b.hot[first].prev = n.prev; // s was the last
        }
    }
    n.parent = n.next = n.prev = kUiNone;
}

int deepest_first(const UiBlock& b, int s) {
    while (b.hot[s].first_child != kUiNone) s = b.hot[s].first_child;
    return s;
}

/// Clears every reference to a freed slot (neighbours, defaults, remembered children, back widgets, modals).
void clear_references(UiBlock& b) {
    auto gone = [&](std::uint16_t& ref) {
        if (ref != kUiNone && b.hot[ref].kind == kUiFreeKind) ref = kUiNone;
    };
    for (int s = 0; s < b.capacity; ++s) {
        if (b.hot[s].kind == kUiFreeKind) continue;
        UiCold& c = b.cold[s];
        for (std::uint16_t& n : c.neighbours) gone(n);
        if (is_container(kind_of(b, s))) {
            gone(c.data.box.def);
            gone(c.data.box.last);
            gone(c.data.box.back);
        }
    }
    gone(b.touch.widget);
    gone(b.touch.scroller);
    gone(b.cursor.target);
    // a modal whose container is gone closes (its saved focus is checked by generation when restored)
    int kept = 0;
    for (int i = 0; i < b.modal_depth; ++i) {
        if (b.modals[i].container != kUiNone && b.hot[b.modals[i].container].kind != kUiFreeKind) {
            b.modals[kept++] = b.modals[i];
        }
    }
    b.modal_depth = kept;
}

} // namespace

int tree_take(UiBlock& b, UiKind kind, int parent) {
    int s = 0;
    while (s < b.capacity && b.hot[s].kind != kUiFreeKind) ++s;
    if (s == b.capacity) return -1;
    UiHot& n = b.hot[s];
    const std::uint16_t generation = static_cast<std::uint16_t>(next_generation(n.generation));
    n = UiHot{};
    n.generation = generation;
    n.kind = static_cast<std::uint8_t>(kind);
    n.flags = kUiVisible | kUiEnabled | kUiInWindow;
    b.cold[s] = UiCold{};
    ++b.live;
    if (parent < 0) return s;
    UiHot& p = b.hot[parent];
    n.flags |= p.flags & kUiBottom;
    n.parent = static_cast<std::uint16_t>(parent);
    if (p.first_child == kUiNone) {
        p.first_child = static_cast<std::uint16_t>(s);
        n.prev = static_cast<std::uint16_t>(s);
    } else {
        const int first = p.first_child;
        const int last = b.hot[first].prev;
        b.hot[last].next = static_cast<std::uint16_t>(s);
        n.prev = static_cast<std::uint16_t>(last);
        b.hot[first].prev = static_cast<std::uint16_t>(s);
    }
    mark_dirty(b, s);
    return s;
}

void tree_destroy(UiBlock& b, int s) {
    mark_dirty(b, s);
    unlink(b, s);
    // post-order: children before parents, the next node found before the current one is freed
    int n = deepest_first(b, s);
    for (;;) {
        int next = -1;
        if (n != s) next = b.hot[n].next != kUiNone ? deepest_first(b, b.hot[n].next) : b.hot[n].parent;
        release_node(b, n);
        b.hot[n].kind = kUiFreeKind;
        b.hot[n].first_child = b.hot[n].next = b.hot[n].prev = b.hot[n].parent = kUiNone;
        b.hot[n].key = 0xFFFF;
        --b.live;
        if (next < 0) break;
        n = next;
    }
    clear_references(b);
}

bool key_taken(const UiBlock& b, std::uint16_t key, int except) {
    if (key == 0xFFFF) return false;
    for (int s = 0; s < b.capacity; ++s) {
        if (s != except && b.hot[s].kind != kUiFreeKind && b.hot[s].key == key) return true;
    }
    return false;
}

bool visible_chain(const UiBlock& b, int s) {
    for (int n = s; n != kUiNone; n = b.hot[n].parent) {
        if ((b.hot[n].flags & kUiVisible) == 0) return false;
    }
    return b.root_visible[root_index_of(b, s)];
}

int preorder_next(const UiBlock& b, int root, int s) {
    if (b.hot[s].first_child != kUiNone) return b.hot[s].first_child;
    while (s != root) {
        if (b.hot[s].next != kUiNone) return b.hot[s].next;
        s = b.hot[s].parent;
    }
    return -1;
}

int preorder_next_visible(const UiBlock& b, int root, int s) {
    int c = b.hot[s].first_child;
    while (c != kUiNone && (b.hot[c].flags & kUiVisible) == 0) c = b.hot[c].next;
    if (c != kUiNone) return c;
    while (s != root) {
        int n = b.hot[s].next;
        while (n != kUiNone && (b.hot[n].flags & kUiVisible) == 0) n = b.hot[n].next;
        if (n != kUiNone) return n;
        s = b.hot[s].parent;
    }
    return -1;
}

} // namespace nnh::detail

namespace nnh {

void Ui::destroy(UiWidget w) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    const int s = detail::slot_of(*b, w);
    if (s < 0) {
        NNH_UI_MISUSE(*b, "Ui::destroy: a stale or null widget");
        return;
    }
    if (detail::kind_of(*b, s) == UiKind::Root) {
        NNH_UI_MISUSE(*b, "Ui::destroy: a root can't be destroyed");
        return;
    }
    bool held = false;
    for (int n = b->focus; n != detail::kUiNone; n = b->hot[n].parent) held = held || n == s;
    const detail::RepairPoint at = detail::repair_point(*b, s);
    detail::tree_destroy(*b, s);
    if (held) detail::set_focus(*b, detail::repair_target(*b, at), false);
}

UiWidget Ui::find(UiKey k) const {
    const detail::UiBlock* b = block_;
    if (b == nullptr || k.none()) return {};
    for (int s = 0; s < b->capacity; ++s) {
        if (b->hot[s].kind != detail::kUiFreeKind && b->hot[s].key == k.value) return detail::handle_of(*b, s);
    }
    return {};
}

} // namespace nnh
