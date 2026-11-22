// Queries on a parsed JSON document (see json.hpp). Parsing: json_parse.cpp.

#include "json.hpp"

namespace nnh::assetc::json {

const Node& Ref::node() const { return doc_->nodes_[index_]; }

Type Ref::type() const { return doc_ ? node().type : Type::Null; }
bool Ref::boolean() const { return is_bool() && node().boolean; }
double Ref::number() const { return is_number() ? node().number : 0.0; }

std::string_view Ref::string() const {
    if (!is_string()) return {};
    return std::string_view(doc_->pool_).substr(node().a, node().b);
}

int Ref::size() const { return is_array() || is_object() ? static_cast<int>(node().b) : 0; }

Ref Ref::first() const { return size() > 0 ? Ref{doc_, node().a} : Ref{}; }

Ref Ref::next() const {
    if (!doc_ || node().next == 0) return {};
    return Ref{doc_, node().next - 1};
}

std::string_view Ref::key() const {
    if (!doc_) return {};
    return std::string_view(doc_->pool_).substr(node().key, node().key_length);
}

Ref Ref::get(std::string_view name) const {
    if (!is_object()) return {};
    for (Ref member = first(); member; member = member.next()) {
        if (member.key() == name) return member;
    }
    return {};
}

} // namespace nnh::assetc::json
