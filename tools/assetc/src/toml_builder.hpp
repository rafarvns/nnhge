#pragma once
// Builds a TOML Document as the parser reads it (toml.cpp): resolves headers against the tables made so far
// and sets keys once, with TOML's rules. Internal to the reader.

#include <string>
#include <vector>

#include "toml_cursor.hpp"

namespace nnh::assetc::toml {

class Builder {
public:
    Builder(Document& doc, Cursor& cursor) : doc_(doc), cursor_(cursor) {}

    /// [a.b] (array false) or [[a.b]]: makes its table the current one. False with the cursor's problem set.
    bool header(const std::vector<std::string>& parts, bool array, int line);
    /// key = value in the current table.
    bool set(const std::string& key, int line, Value value);

private:
    int add_table(const std::string& path, int line, bool defined);
    int add_entry(int table, const std::string& key, int line, EntryKind kind);

    Document& doc_;
    Cursor& cursor_;
    int current_ = 0;
};

} // namespace nnh::assetc::toml
