// The references sidecar (data_refs.hpp).

#include "data_refs.hpp"

#include "assets/asset_path.hpp"
#include "data/data_format.hpp"

namespace nnh::assetc {

namespace {

const char* kind_name(detail::FieldKind kind) {
    switch (kind) {
    case detail::FieldKind::TextureRef:
        return "texture";
    case detail::FieldKind::SoundRef:
        return "sound";
    case detail::FieldKind::MusicRef:
        return "music";
    default:
        return "data";
    }
}

} // namespace

std::string refs_text(std::string_view name, std::string_view key, std::size_t size,
                      const std::vector<DataReference>& references) {
    std::string out = kRefsHeader;
    out += "asset " + std::string(key) + " " + std::to_string(size) + " " + std::string(name) + "\n";
    for (const DataReference& r : references) {
        out += "ref " + std::to_string(r.line) + " " + kind_name(r.kind) + " " +
               (r.target.empty() ? "-" : r.target) + " " + r.field + " " + r.path + "\n";
    }
    return out;
}

std::string empty_refs_text() {
    return kRefsHeader;
}

namespace {

/// Splits the first `count` space-separated tokens off `line`; the rest (possibly with spaces) goes last.
bool split(std::string_view line, int count, std::vector<std::string_view>& out) {
    out.clear();
    for (int i = 0; i < count; ++i) {
        const std::size_t space = line.find(' ');
        if (space == std::string_view::npos || space == 0) return false;
        out.push_back(line.substr(0, space));
        line.remove_prefix(space + 1);
    }
    if (line.empty()) return false;
    out.push_back(line);
    return true;
}

bool number(std::string_view text, std::size_t max, std::size_t& out) {
    if (text.empty() || text.size() > 6 || (text.size() > 1 && text[0] == '0')) return false;
    out = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        out = out * 10 + static_cast<std::size_t>(c - '0');
    }
    return out <= max;
}

bool key_ok(std::string_view key) {
    if (key.empty() || key.size() > detail::datafmt::kMaxKeyLength || key[0] < 'a' || key[0] > 'z') return false;
    for (char c : key) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

/// The asset path rules the engine looks paths up with (and the index validator checks). A NUL inside would
/// hide the rest of the path from that check (found by fuzzing).
bool path_ok(std::string_view path) {
    return path.find('\0') == std::string_view::npos && detail::valid_asset_path(std::string(path).c_str());
}

bool kind_of(std::string_view name, detail::FieldKind& kind) {
    if (name == "data") kind = detail::FieldKind::DataRef;
    else if (name == "texture") kind = detail::FieldKind::TextureRef;
    else if (name == "sound") kind = detail::FieldKind::SoundRef;
    else if (name == "music") kind = detail::FieldKind::MusicRef;
    else return false;
    return true;
}

/// One record after the header; "" when it is fine, else the reason.
std::string read_record(std::string_view line, RefsFile& out) {
    std::vector<std::string_view> t;
    const std::string_view record = line.substr(0, line.find(' '));
    if (record == "asset") {
        if (out.data) return "a second 'asset' line";
        if (!split(line, 3, t)) return "expected 'asset <key> <size> <name>'";
        if (!key_ok(t[1])) return "bad key '" + std::string(t[1]) + "'";
        if (!number(t[2], detail::datafmt::kMaxFileBytes, out.size)) return "bad size '" + std::string(t[2]) + "'";
        if (!path_ok(t[3])) return "bad asset path '" + std::string(t[3]) + "'";
        out.data = true;
        out.key = t[1];
        out.name = t[3];
        return "";
    }
    if (record != "ref") return "unknown record '" + std::string(record) + "'";
    if (!out.data) return "'ref' before 'asset'";
    if (!split(line, 5, t)) return "expected 'ref <line> <kind> <type or -> <field> <path>'";
    DataReference r;
    std::size_t at = 0;
    if (!number(t[1], 999999, at) || at == 0) return "bad line '" + std::string(t[1]) + "'";
    r.line = static_cast<int>(at);
    if (!kind_of(t[2], r.kind)) return "bad kind '" + std::string(t[2]) + "'";
    if (r.kind == detail::FieldKind::DataRef && !key_ok(t[3])) return "bad key '" + std::string(t[3]) + "'";
    if (r.kind != detail::FieldKind::DataRef && t[3] != "-")
        return "a " + std::string(t[2]) + " reference has no type";
    if (r.kind == detail::FieldKind::DataRef) r.target = t[3];
    if (!path_ok(t[5])) return "bad asset path '" + std::string(t[5]) + "'";
    r.field = t[4];
    r.path = t[5];
    out.references.push_back(std::move(r));
    return "";
}

} // namespace

bool read_refs(std::string_view text, RefsFile& out, std::string& error) {
    out = RefsFile{};
    const std::string_view header = kRefsHeader;
    if (text.substr(0, header.size()) != header) {
        error = "line 1: expected 'nnh-data-refs 1'";
        return false;
    }
    text.remove_prefix(header.size());
    for (int n = 2; !text.empty(); ++n) {
        const std::size_t lf = text.find('\n');
        std::string reason = lf == std::string_view::npos ? "the last line has no LF"
                                                          : read_record(text.substr(0, lf), out);
        if (!reason.empty()) {
            error = "line " + std::to_string(n) + ": " + reason;
            return false;
        }
        text.remove_prefix(lf + 1);
    }
    return true;
}

} // namespace nnh::assetc
