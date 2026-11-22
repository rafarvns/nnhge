// Reading a schema description (schema_read.hpp).

#include "schema_read.hpp"

#include <cfloat>
#include <charconv>
#include <cstdio>

#include "data/data_format.hpp"
#include "data/schema_check.hpp"
#include "data/schema_fingerprint.hpp"
#include "nnh/data.hpp"

namespace nnh::assetc {

using detail::FieldInfo;
using detail::FieldKind;
using detail::FieldShape;

namespace {

std::vector<std::string_view> split(std::string_view line) {
    std::vector<std::string_view> out;
    std::size_t at = 0;
    while (at <= line.size()) {
        std::size_t end = line.find(' ', at);
        if (end == std::string_view::npos) end = line.size();
        out.push_back(line.substr(at, end - at));
        at = end + 1;
    }
    return out;
}

template <class T> bool number(std::string_view s, T& out) {
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size();
}

struct PlainKind {
    const char* name;
    FieldKind kind;
    std::uint8_t width;
    bool is_signed;
};
constexpr PlainKind kPlainKinds[] = {
    {"bool", FieldKind::Bool, 1, false},
    {"i8", FieldKind::Int, 1, true},
    {"u8", FieldKind::Int, 1, false},
    {"i16", FieldKind::Int, 2, true},
    {"u16", FieldKind::Int, 2, false},
    {"i32", FieldKind::Int, 4, true},
    {"u32", FieldKind::Int, 4, false},
    {"float", FieldKind::Float, 4, false},
    {"vec2", FieldKind::Vec2, 0, false},
    {"color", FieldKind::Color, 0, false},
    {"texture", FieldKind::TextureRef, 0, false},
    {"sound", FieldKind::SoundRef, 0, false},
    {"music", FieldKind::MusicRef, 0, false},
};

/// The range the member's type holds (the effective range when the description writes none).
void type_range(const FieldInfo& f, double& lowest, double& highest) {
    if (f.kind == FieldKind::Float) {
        lowest = -static_cast<double>(FLT_MAX);
        highest = static_cast<double>(FLT_MAX);
    } else {
        const double span = static_cast<double>(1ull << (8 * f.width));
        lowest = f.is_signed ? -span / 2 : 0;
        highest = f.is_signed ? span / 2 - 1 : span - 1;
    }
}

} // namespace

class SchemaReader {
public:
    SchemaReader(Schema& s, std::string& error) : s_(s), error_(error) {}

    bool read(std::string_view text);

private:
    bool fail(const std::string& why) {
        error_ = "line " + std::to_string(line_) + ": " + why;
        return false;
    }
    const char* keep(std::string_view text) { return s_.strings_.emplace_back(text).c_str(); }
    static bool name_ok(std::string_view name) {
        return !name.empty() && name.size() <= static_cast<std::size_t>(detail::datafmt::kMaxNameLength);
    }
    bool missing_fields() {
        const int declared = s_.types_.back().field_count;
        return fail("type " + type_name_ + ": expected " + std::to_string(declared) + " fields, found " +
                    std::to_string(declared - fields_left_));
    }

    bool record(const std::vector<std::string_view>& t, std::string_view line);
    bool enum_record(const std::vector<std::string_view>& t);
    bool type_record(const std::vector<std::string_view>& t, std::string_view line);
    bool field_record(const std::vector<std::string_view>& t);
    bool kind(const std::vector<std::string_view>& t, std::size_t& at, FieldInfo& f);
    bool bounds(const std::vector<std::string_view>& t, std::size_t at, FieldInfo& f);
    bool resolve();
    bool check();

    struct Pending {
        FieldInfo* field;
        std::string name;
        bool is_enum;
        int line;
    };
    Schema& s_;
    std::string& error_;
    int line_ = 0;
    int fields_left_ = 0; ///< of the type being read
    std::string type_name_;
    std::vector<Pending> pending_;
};

bool SchemaReader::read(std::string_view text) {
    if (text.size() > kMaxDescriptionBytes) {
        error_ = "schema: the description is larger than 1 MiB";
        return false;
    }
    std::size_t at = 0;
    while (at < text.size() || line_ == 0) {
        std::size_t end = text.find('\n', at);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view line = text.substr(at, end - at);
        at = end + 1;
        ++line_;
        if (line_ == 1) {
            if (line != "nnh-data-schema 1") return fail("expected 'nnh-data-schema 1'");
            continue;
        }
        if (line.empty()) continue;
        if (!record(split(line), line)) return false;
    }
    ++line_; // where the next field was expected
    if (fields_left_ > 0) return missing_fields();
    return resolve() && check();
}

bool SchemaReader::record(const std::vector<std::string_view>& t, std::string_view line) {
    if (t[0] == "field") {
        if (fields_left_ == 0) return fail("a field outside a type");
        return field_record(t);
    }
    if (fields_left_ > 0) return missing_fields();
    if (t[0] == "enum") return enum_record(t);
    if (t[0] == "type") return type_record(t, line);
    return fail("unknown record '" + std::string(t[0]) + "'");
}

bool SchemaReader::enum_record(const std::vector<std::string_view>& t) {
    int count = 0;
    if (t.size() < 3 || !name_ok(t[1])) return fail("a name is longer than 63 characters");
    if (!number(t[2], count) || count < 1 || count > detail::datafmt::kMaxEnumNames) {
        return fail("enum " + std::string(t[1]) + ": bad name count '" + std::string(t[2]) + "'");
    }
    if (static_cast<int>(s_.enums_.size()) == detail::datafmt::kMaxEnums) return fail("more than 64 enums");
    if (t.size() - 3 != static_cast<std::size_t>(count)) {
        return fail("enum " + std::string(t[1]) + ": expected " + std::to_string(count) + " names, found " +
                    std::to_string(t.size() - 3));
    }
    std::string names;
    std::vector<std::int64_t>& values = s_.enum_values_.emplace_back();
    for (int i = 0; i < count; ++i) {
        if (!name_ok(t[3 + i])) return fail("a name is longer than 63 characters");
        names += (i == 0 ? "" : ", ") + std::string(t[3 + i]);
        values.push_back(i);
    }
    detail::EnumInfo& e = s_.enums_.emplace_back();
    e.tag = &e;
    e.name = keep(t[1]);
    e.names = keep(names);
    e.values = values.data();
    e.source = "";
    e.count = static_cast<std::uint16_t>(count);
    e.width = 1;
    detail::add_enum(s_.registry_, s_.enum_nodes_.emplace_back(detail::EnumNode{&e, nullptr}));
    return true;
}

bool SchemaReader::type_record(const std::vector<std::string_view>& t, std::string_view line) {
    if (t.size() < 6) return fail("a type needs a name, a key, a field count, a fingerprint and a source");
    if (!name_ok(t[1]) || t[2].size() > static_cast<std::size_t>(detail::datafmt::kMaxKeyLength)) {
        return fail("a name is longer than 63 characters");
    }
    if (static_cast<int>(s_.types_.size()) == detail::datafmt::kMaxTypes) return fail("more than 128 types");
    int fields = 0;
    std::uint32_t fingerprint = 0;
    if (!number(t[3], fields) || fields < 0 || fields > nnh::data::kMaxFields) {
        return fail("type " + std::string(t[1]) + ": bad field count '" + std::string(t[3]) + "'");
    }
    const auto fp = std::from_chars(t[4].data(), t[4].data() + t[4].size(), fingerprint, 16);
    if (t[4].size() != 8 || fp.ptr != t[4].data() + 8)
        return fail("bad fingerprint '" + std::string(t[4]) + "'");
    const std::size_t source_at = static_cast<std::size_t>(t[5].data() - line.data());
    detail::TypeInfo& type = s_.types_.emplace_back();
    std::vector<FieldInfo>& list = s_.fields_.emplace_back();
    list.reserve(static_cast<std::size_t>(fields)); // addresses stay put while the fields are read
    type.tag = &type;
    type.name = keep(t[1]);
    type.key = t[2] == "-" ? "" : keep(t[2]);
    type.source = keep(line.substr(source_at));
    type.fields = list.data();
    type.field_count = static_cast<std::uint16_t>(fields);
    s_.fingerprints_.push_back(fingerprint);
    detail::add_type(s_.registry_, s_.type_nodes_.emplace_back(detail::TypeNode{&type, nullptr}));
    fields_left_ = fields;
    type_name_ = std::string(t[1]);
    return true;
}

bool SchemaReader::field_record(const std::vector<std::string_view>& t) {
    if (t.size() < 4) return fail("a field needs a name, a shape and a kind");
    if (!name_ok(t[1])) return fail("a name is longer than 63 characters");
    FieldInfo f;
    f.name = keep(t[1]);
    std::size_t at = 2;
    if (t[at] == "array" || t[at] == "list") {
        f.shape = t[at] == "array" ? FieldShape::Array : FieldShape::List;
        int count = 0;
        if (t.size() < 5 || !number(t[at + 1], count) || count < 1 || count > nnh::data::kMaxListItems) {
            return fail("bad count '" + std::string(t[at + 1]) + "'");
        }
        f.count = static_cast<std::uint16_t>(count);
        at += 2;
    } else if (t[at] == "one") {
        ++at;
    } else {
        return fail("unknown field shape '" + std::string(t[at]) + "'");
    }
    std::vector<FieldInfo>& list = s_.fields_.back();
    list.push_back(f);
    if (!kind(t, at, list.back()) || !bounds(t, at, list.back())) return false;
    --fields_left_;
    return true;
}

bool SchemaReader::kind(const std::vector<std::string_view>& t, std::size_t& at, FieldInfo& f) {
    const std::string_view k = at < t.size() ? t[at] : std::string_view{};
    ++at;
    for (const PlainKind& p : kPlainKinds) {
        if (k != p.name) continue;
        f.kind = p.kind;
        f.width = p.width;
        f.is_signed = p.is_signed;
        if (f.kind == FieldKind::Int || f.kind == FieldKind::Float) type_range(f, f.min, f.max);
        return true;
    }
    const std::string_view arg = at < t.size() ? t[at] : std::string_view{};
    if (k == "string") {
        int capacity = 0;
        if (!number(arg, capacity) || capacity < 1 || capacity > nnh::data::kMaxStringBytes) {
            return fail("bad capacity '" + std::string(arg) + "'");
        }
        f.kind = FieldKind::String;
        f.capacity = static_cast<std::uint16_t>(capacity);
    } else if (k == "enum" || k == "struct" || k == "ref") {
        f.kind = k == "enum" ? FieldKind::Enum : k == "struct" ? FieldKind::Struct : FieldKind::DataRef;
        f.width = k == "enum" ? 1 : 0;
        pending_.push_back({&f, std::string(arg), k == "enum", line_});
    } else {
        return fail("unknown field kind '" + std::string(k) + "'");
    }
    ++at;
    return true;
}

bool SchemaReader::bounds(const std::vector<std::string_view>& t, std::size_t at, FieldInfo& f) {
    while (at < t.size()) {
        if (t[at] == "required") {
            f.required = true;
            ++at;
            continue;
        }
        if (t[at] != "min" && t[at] != "max") return fail("unexpected '" + std::string(t[at]) + "'");
        if (f.kind != FieldKind::Int && f.kind != FieldKind::Float)
            return fail("min/max on a field that isn't a number");
        double v = 0;
        if (at + 1 >= t.size() || !number(t[at + 1], v)) {
            return fail("bad number '" + std::string(at + 1 < t.size() ? t[at + 1] : "") + "'");
        }
        (t[at] == "min" ? f.min : f.max) = v;
        at += 2;
    }
    return true;
}

bool SchemaReader::resolve() {
    for (const Pending& p : pending_) {
        line_ = p.line;
        if (p.is_enum) {
            const detail::EnumInfo* e = detail::find_enum_by_name(s_.registry_, p.name.c_str());
            if (e == nullptr) return fail("unknown enum '" + p.name + "'");
            p.field->target = e->tag;
        } else {
            const detail::TypeInfo* type = detail::find_type_by_name(s_.registry_, p.name.c_str());
            if (type == nullptr) return fail("unknown type '" + p.name + "'");
            p.field->target = type->tag;
        }
    }
    return true;
}

bool SchemaReader::check() {
    std::string first;
    const auto sink = [](void* context, const char* message) {
        auto* out = static_cast<std::string*>(context);
        if (out->empty()) *out = message;
    };
    if (detail::check_schema(s_.registry_, sink, &first) > 0) {
        error_ = "schema: " + first;
        return false;
    }
    for (std::size_t i = 0; i < s_.types_.size(); ++i) {
        const std::uint32_t computed = detail::schema_fingerprint(s_.registry_, s_.types_[i]);
        if (computed == s_.fingerprints_[i]) continue;
        char text[160];
        std::snprintf(
            text, sizeof text, "schema: type %s: the description says %08X, nnh-assetc computes %08X",
            s_.types_[i].name, static_cast<unsigned>(s_.fingerprints_[i]), static_cast<unsigned>(computed));
        error_ = text;
        return false;
    }
    return true;
}

const detail::TypeInfo* Schema::type_by_key(std::string_view key) const {
    if (key.empty()) return nullptr;
    for (const detail::TypeInfo& t : types_) {
        if (key == t.key) return &t;
    }
    return nullptr;
}

const detail::TypeInfo* Schema::type_by_name(std::string_view name) const {
    for (const detail::TypeInfo& t : types_) {
        if (name == t.name) return &t;
    }
    return nullptr;
}

std::uint32_t Schema::fingerprint(const detail::TypeInfo& type) const {
    for (std::size_t i = 0; i < types_.size(); ++i) {
        if (&types_[i] == &type) return fingerprints_[i];
    }
    return 0;
}

bool read_schema(std::string_view text, Schema& out, std::string& error) {
    error.clear();
    return SchemaReader(out, error).read(text);
}

} // namespace nnh::assetc
