// The references sidecar (data_refs.hpp).

#include "data_refs.hpp"

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

} // namespace nnh::assetc
