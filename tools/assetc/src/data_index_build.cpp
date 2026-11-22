// The data index step (data_index_build.hpp).

#include "data_index_build.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

#include "data/index_format.hpp"
#include "nnh/detail/name_hash.hpp"
#include "suggest.hpp"

namespace nnh::assetc {

namespace fmt = detail::idxfmt;

namespace {

/// "an item", "a sound".
std::string with_article(const std::string& word) {
    const bool vowel = !word.empty() && std::string_view("aeiou").find(word[0]) != std::string_view::npos;
    return (vowel ? "an " : "a ") + word;
}

std::string hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof text, "0x%08X", static_cast<unsigned>(value));
    return text;
}

int group_of(detail::FieldKind kind) {
    return kind == detail::FieldKind::TextureRef ? 0 : kind == detail::FieldKind::SoundRef ? 1 : 2;
}

/// What the build knows about every path: data assets by name, images and audio by path without extension.
class Catalog {
public:
    Catalog(const IndexInput& in, const std::vector<const RefsFile*>& data) {
        for (const RefsFile* f : data)
            data_.emplace(f->name, f->key); // the first of a duplicate name (reported separately)
        for (const std::string& a : in.assets) {
            if (ends_with(a, ".png")) images_.insert(a.substr(0, a.size() - 4));
            if (ends_with(a, ".wav") || ends_with(a, ".ogg")) audio_.insert(a.substr(0, a.size() - 4));
        }
    }

    bool image(const std::string& p) const { return images_.count(p) != 0; }
    bool audio(const std::string& p) const { return audio_.count(p) != 0; }
    /// The key of the data asset `p`, or "".
    std::string data_key(const std::string& p) const {
        const auto found = data_.find(p);
        return found == data_.end() ? "" : found->second;
    }
    /// What `p` is, for "is X, not Y" ("" if nothing).
    std::string what(const std::string& p) const {
        if (!data_key(p).empty()) return with_article(data_key(p));
        if (image(p)) return "an image";
        if (audio(p)) return "a sound";
        return "";
    }
    std::vector<std::string> data_of(const std::string& key) const {
        std::vector<std::string> out;
        for (const auto& [name, k] : data_)
            if (k == key) out.push_back(name);
        return out;
    }
    std::vector<std::string> images() const { return {images_.begin(), images_.end()}; }
    std::vector<std::string> audio() const { return {audio_.begin(), audio_.end()}; }

private:
    static bool ends_with(const std::string& s, const char* tail) {
        return s.size() > 4 && s.compare(s.size() - 4, 4, tail) == 0;
    }
    std::map<std::string, std::string> data_;
    std::set<std::string> images_, audio_;
};

/// " (no file <shown>/<p><ext>; did you mean '<closest>'?)"
std::string no_file(const IndexInput& in, const std::string& p, const char* ext,
                    const std::vector<std::string>& candidates) {
    const std::string near = closest(p, candidates);
    return " (no file " + in.shown + "/" + p + ext + (near.empty() ? "" : "; did you mean '" + near + "'?") + ")";
}

/// The problem of one reference, or "".
std::string check_reference(const IndexInput& in, const Catalog& c, const DataReference& r) {
    const std::string& p = r.path;
    if (r.kind == detail::FieldKind::DataRef) {
        const std::string key = c.data_key(p);
        if (key == r.target) return "";
        if (in.schema->type_by_key(r.target) == nullptr) return "the schema has no data type '" + r.target + "'";
        if (!c.what(p).empty()) return "'" + p + "' is " + c.what(p) + ", not " + with_article(r.target);
        return "no " + r.target + " '" + p + "'" + no_file(in, p, ("." + r.target).c_str(), c.data_of(r.target));
    }
    const bool texture = r.kind == detail::FieldKind::TextureRef;
    if (texture ? c.image(p) : c.audio(p)) return "";
    const std::string wanted = texture ? "a texture" : r.kind == detail::FieldKind::SoundRef ? "a sound" : "music";
    if (!c.what(p).empty()) return "'" + p + "' is " + c.what(p) + ", not " + wanted;
    const std::string noun = texture ? "texture" : r.kind == detail::FieldKind::SoundRef ? "sound" : "music";
    return "no " + noun + " '" + p + "'" +
           no_file(in, p, texture ? ".png" : ".wav or .ogg", texture ? c.images() : c.audio());
}

struct Problems {
    std::map<std::string, Diagnostics> files; ///< by shown path
    std::vector<std::string> global;

    void add(const IndexInput& in, const RefsFile& f, int line, std::string message) {
        files[in.shown + "/" + f.name + "." + f.key].add(line, std::move(message));
    }

    std::string text(const IndexInput& in) const {
        std::string out;
        for (const std::string& g : global)
            out += in.shown + ": " + g + "\n";
        if (!out.empty()) return out;
        for (const auto& [shown, d] : files)
            if (!d.empty()) return d.text(shown);
        return "";
    }
};

/// Names unique across types, known types, hashes unique and non-zero.
void check_names(const IndexInput& in, const std::vector<const RefsFile*>& data, Problems& out) {
    std::map<std::string, const RefsFile*> first;
    std::map<std::uint32_t, std::string> hashes;
    for (const RefsFile* f : data) {
        if (in.schema->type_by_key(f->key) == nullptr) {
            out.add(in, *f, 1, "the schema has no data type '" + f->key + "': rebuild the assets");
        }
        const auto [at, added] = first.emplace(f->name, f);
        if (!added) {
            out.add(in, *f, 1, "the data asset name '" + f->name + "' is also used by " + in.shown + "/" +
                                   f->name + "." + at->second->key + ": names must be unique across types");
            continue;
        }
        const std::uint32_t h = in.hash(f->name);
        if (h == 0) {
            out.add(in, *f, 1, "the name '" + f->name + "' hashes to 0: rename the asset");
        } else if (!hashes.emplace(h, f->name).second) {
            out.add(in, *f, 1, "the names '" + hashes[h] + "' and '" + f->name + "' have the same hash " + hex(h) +
                                   ": rename one of them");
        }
    }
}

void check_media_hashes(const IndexInput& in, const std::vector<std::string> (&media)[3], Problems& out) {
    for (const std::vector<std::string>& group : media) {
        std::map<std::uint32_t, std::string> hashes;
        for (const std::string& p : group) {
            const std::uint32_t h = in.hash(p);
            if (h == 0) {
                out.global.push_back("the path '" + p + "' hashes to 0: rename the asset");
            } else if (!hashes.emplace(h, p).second) {
                out.global.push_back("the paths '" + hashes[h] + "' and '" + p + "' have the same hash " + hex(h) +
                                     ": rename one of them");
            }
        }
    }
}

void check_limits(const IndexModel& m, std::size_t data_count, Problems& out) {
    if (data_count > static_cast<std::size_t>(fmt::kMaxDataEntries)) {
        out.global.push_back(std::to_string(data_count) + " data assets: at most " +
                             std::to_string(fmt::kMaxDataEntries));
        return;
    }
    const std::size_t media = m.media[0].size() + m.media[1].size() + m.media[2].size();
    if (media > static_cast<std::size_t>(fmt::kMaxMediaEntries)) {
        out.global.push_back(std::to_string(media) + " textures, sounds and music referenced by data assets: "
                             "at most " + std::to_string(fmt::kMaxMediaEntries));
        return;
    }
    std::size_t strings = 0;
    for (const IndexModel::Type& t : m.types)
        for (const std::string& p : t.paths)
            strings += p.size() + 1;
    for (const std::vector<std::string>& group : m.media)
        for (const std::string& p : group)
            strings += p.size() + 1;
    if (strings > fmt::kMaxStringBytes) {
        out.global.push_back("the data index's paths take " + std::to_string(strings) + " bytes: at most " +
                             std::to_string(fmt::kMaxStringBytes));
    }
}

} // namespace

std::uint32_t fnv1a_path(std::string_view path) {
    return detail::fnv1a32(path.data(), path.size());
}

bool build_index(const IndexInput& in, IndexModel& out, std::string& errors) {
    out = IndexModel{};
    std::vector<const RefsFile*> data;
    for (const RefsFile& f : in.data)
        if (f.data) data.push_back(&f);
    std::stable_sort(data.begin(), data.end(), [](const RefsFile* a, const RefsFile* b) {
        return a->name != b->name ? a->name < b->name : a->key < b->key;
    });
    Problems problems;
    check_names(in, data, problems);
    const Catalog catalog(in, data);
    std::set<std::string> media[3];
    for (const RefsFile* f : data) {
        for (const DataReference& r : f->references) {
            const std::string problem = check_reference(in, catalog, r);
            if (!problem.empty()) problems.add(in, *f, r.line, r.field + ": " + problem);
            else if (r.kind != detail::FieldKind::DataRef) media[group_of(r.kind)].insert(r.path);
        }
        out.largest = std::max(out.largest, static_cast<std::uint32_t>(f->size));
    }
    for (const detail::TypeNode* n = in.schema->registry().types; n != nullptr; n = n->next) {
        if (n->info->key[0] == '\0') continue;
        IndexModel::Type t{n->info->key, in.schema->fingerprint(*n->info), catalog.data_of(n->info->key)};
        out.types.push_back(std::move(t));
    }
    std::sort(out.types.begin(), out.types.end(),
              [](const IndexModel::Type& a, const IndexModel::Type& b) { return a.key < b.key; });
    for (int g = 0; g < 3; ++g)
        out.media[g].assign(media[g].begin(), media[g].end());
    check_media_hashes(in, out.media, problems);
    check_limits(out, data.size(), problems);
    errors = problems.text(in);
    return errors.empty();
}

} // namespace nnh::assetc
