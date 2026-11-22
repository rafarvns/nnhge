// The TOML document tree (toml.hpp): headers resolved with TOML's rules, keys set once.

#include "toml_builder.hpp"

namespace nnh::assetc::toml {

const Entry* Table::find(std::string_view key) const {
    for (const Entry& e : entries) {
        if (e.key == key) return &e;
    }
    return nullptr;
}

namespace {

int find_index(const Table& t, std::string_view key) {
    for (std::size_t i = 0; i < t.entries.size(); ++i) {
        if (t.entries[i].key == key) return static_cast<int>(i);
    }
    return -1;
}

std::string at_line(int line) {
    return " (line " + std::to_string(line) + ")";
}

} // namespace

int Builder::add_table(const std::string& path, int line, bool defined) {
    Table t;
    t.path = path;
    t.line = line;
    t.defined = defined;
    doc_.tables_.push_back(std::move(t));
    return static_cast<int>(doc_.tables_.size()) - 1;
}

int Builder::add_entry(int table, const std::string& key, int line, EntryKind kind) {
    Entry e;
    e.key = key;
    e.line = line;
    e.kind = kind;
    std::vector<Entry>& entries = doc_.tables_[static_cast<std::size_t>(table)].entries;
    entries.push_back(std::move(e));
    return static_cast<int>(entries.size()) - 1;
}

bool Builder::header(const std::vector<std::string>& parts, bool array, int line) {
    int t = 0;
    std::string path;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const std::string& key = parts[i];
        path += (i == 0 ? "" : ".") + key;
        const bool last = i + 1 == parts.size();
        const int found = find_index(doc_.tables_[static_cast<std::size_t>(t)], key);
        if (found < 0) {
            const int made = add_table(path, last ? line : 0, last);
            const int e = add_entry(t, key, line, last && array ? EntryKind::TableArray : EntryKind::Table);
            Entry& entry = doc_.tables_[static_cast<std::size_t>(t)].entries[static_cast<std::size_t>(e)];
            if (entry.kind == EntryKind::TableArray)
                entry.elements.push_back(made);
            else
                entry.table = made;
            t = made;
            continue;
        }
        Entry& e = doc_.tables_[static_cast<std::size_t>(t)].entries[static_cast<std::size_t>(found)];
        const std::string name = "'" + key + "'";
        if (e.kind == EntryKind::Value) {
            return cursor_.fail(name + " is already a value" + at_line(e.line) + ", not a" +
                                (last && array ? "n array of tables" : " table"));
        }
        if (!last) {
            t = e.kind == EntryKind::Table ? e.table : e.elements.back();
        } else if (array) {
            if (e.kind != EntryKind::TableArray) {
                return cursor_.fail(name + " is a table" + at_line(e.line) + ", not an array of tables");
            }
            const int made = add_table(path, line, true);
            doc_.tables_[static_cast<std::size_t>(t)]
                .entries[static_cast<std::size_t>(found)]
                .elements.push_back(made);
            t = made;
        } else {
            if (e.kind != EntryKind::Table) {
                return cursor_.fail(name + " is an array of tables" + at_line(e.line) + ", not a table");
            }
            Table& existing = doc_.tables_[static_cast<std::size_t>(e.table)];
            if (existing.defined) {
                return cursor_.fail("[" + path + "] is defined twice (first on line " +
                                    std::to_string(existing.line) + ")");
            }
            existing.defined = true;
            existing.line = line;
            t = e.table;
        }
    }
    current_ = t;
    return true;
}

bool Builder::set(const std::string& key, int line, Value value) {
    const Table& t = doc_.tables_[static_cast<std::size_t>(current_)];
    const Entry* before = t.find(key);
    if (before != nullptr) {
        return cursor_.fail("'" + key + "' is set twice (first on line " + std::to_string(before->line) +
                            ")");
    }
    const int e = add_entry(current_, key, line, EntryKind::Value);
    doc_.tables_[static_cast<std::size_t>(current_)].entries[static_cast<std::size_t>(e)].value =
        std::move(value);
    return true;
}

} // namespace nnh::assetc::toml
