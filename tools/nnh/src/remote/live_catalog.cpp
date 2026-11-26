// The PC's mirror of a game's live values (see live_catalog.hpp).

#include "remote/live_catalog.hpp"

#include <algorithm>

namespace nnh::tool::remote {
namespace {

std::size_t distance(const std::string& a, const std::string& b) {
    std::vector<std::size_t> row(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) row[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        std::size_t diag = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t up = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diag + (a[i - 1] == b[j - 1] ? 0u : 1u)});
            diag = up;
        }
    }
    return row[b.size()];
}

} // namespace

bool LiveCatalog::add_entry(const std::uint8_t* payload, std::uint32_t length) {
    detail::remote::LiveEntryView v;
    if (!detail::remote::decode_live_entry(payload, length, v)) return false;
    LiveItem item;
    item.name.assign(v.name, v.header.name_length);
    item.hash = v.header.hash;
    item.kind = v.header.kind;
    item.flags = v.header.flags;
    item.value = v.header.value;
    item.def = v.header.def;
    item.min = v.header.min;
    item.max = v.header.max;
    item.step = v.header.step;
    for (int i = 0; i < v.header.enum_count; ++i) {
        char name[detail::remote::kMaxEnumNameBytes + 1];
        detail::remote::live_entry_enum_name(v, i, name);
        item.enum_names.emplace_back(name);
    }
    remove(item.hash);
    const auto at = std::lower_bound(items_.begin(), items_.end(), item.name,
                                     [](const LiveItem& a, const std::string& n) { return a.name < n; });
    items_.insert(at, std::move(item));
    return true;
}

void LiveCatalog::remove(std::uint32_t hash) {
    items_.erase(std::remove_if(items_.begin(), items_.end(), [&](const LiveItem& i) { return i.hash == hash; }),
                 items_.end());
}

void LiveCatalog::clear() {
    items_.clear();
    complete = false;
}

const LiveItem* LiveCatalog::find(const std::string& name) const {
    for (const LiveItem& i : items_) {
        if (i.name == name) return &i;
    }
    return nullptr;
}

const LiveItem* LiveCatalog::find(std::uint32_t hash) const {
    for (const LiveItem& i : items_) {
        if (i.hash == hash) return &i;
    }
    return nullptr;
}

std::vector<const LiveItem*> LiveCatalog::list(const std::string& group) const {
    std::vector<const LiveItem*> out;
    for (const LiveItem& i : items_) {
        if (group.empty() || i.name.compare(0, group.size() + 1, group + "/") == 0) out.push_back(&i);
    }
    return out;
}

std::string LiveCatalog::closest(const std::string& name) const {
    std::string best;
    std::size_t best_d = 4;
    for (const LiveItem& i : items_) {
        const std::size_t d = distance(name, i.name);
        if (d < best_d) {
            best_d = d;
            best = i.name;
        }
    }
    return best;
}

bool LiveCatalog::is_group(const std::string& name) const { return !name.empty() && !list(name).empty(); }

} // namespace nnh::tool::remote
