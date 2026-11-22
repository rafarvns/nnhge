// Fuzz target bodies for `nnh-assetc data` and `data-index` (see fuzz_assetc.hpp): a whole conversion from
// memory against a fixed schema with every field kind (declared here like a game's, read back through its
// description), checked with the engine's validator and applied into the compiled struct; after a NUL byte, a
// schema description of its own. And a references sidecar read back and indexed against the same schema.

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "assets/asset_path.hpp"
#include "data/data_apply.hpp"
#include "data/data_validate.hpp"
#include "data/schema_fingerprint.hpp"
#include "data/schema_write.hpp"
#include "data_convert.hpp"
#include "data_index_build.hpp"
#include "data_refs.hpp"
#include "fuzz_assetc.hpp"
#include "nnh/data.hpp"
#include "nnh/detail/name_hash.hpp"
#include "nnh/gfx.hpp"

namespace nnh::assetc::fuzz {

namespace {

using detail::FieldInfo;
using detail::TypeInfo;

enum class Kind : std::int8_t { Low = -1, Mid = 4, High = 9 };
struct Part {
    std::uint16_t hp = 5;
    nnh::data::Ref<Part> link;
};
struct All {
    bool flag = false;
    std::int8_t i8 = 0;
    std::uint32_t u32 = 0;
    float ratio = 0;
    nnh::data::String<8> name;
    Kind kind = Kind::Mid;
    Vec2 at;
    Color tint;
    Part part;
    nnh::data::TextureRef sprite;
    nnh::data::SoundRef sound;
    nnh::data::MusicRef music;
    std::int16_t row[3] = {};
    nnh::data::List<Part, 3> parts;
    nnh::data::List<Vec2, 2> path;
};

struct PartFields {
    using Self = Part;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(hp, .min = 1, .required = true),
                                         NNH_DATA_FIELD(link)};
};
struct AllFields {
    using Self = All;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(flag),  NNH_DATA_FIELD(i8, .min = -50),
                                         NNH_DATA_FIELD(u32),   NNH_DATA_FIELD(ratio, .max = 2),
                                         NNH_DATA_FIELD(name),  NNH_DATA_FIELD(kind),
                                         NNH_DATA_FIELD(at),    NNH_DATA_FIELD(tint),
                                         NNH_DATA_FIELD(part),  NNH_DATA_FIELD(sprite),
                                         NNH_DATA_FIELD(sound), NNH_DATA_FIELD(music),
                                         NNH_DATA_FIELD(row),   NNH_DATA_FIELD(parts),
                                         NNH_DATA_FIELD(path)};
};
constexpr detail::DataEnumValues<3> kKindValues{{-1, 4, 9}};
constexpr detail::EnumInfo kKind = detail::make_enum<Kind>("Kind", "Low, Mid, High", kKindValues, "fuzz");
constexpr TypeInfo kPart = detail::make_type<Part>("Part", "part", "fuzz", PartFields::list);
constexpr TypeInfo kAll = detail::make_type<All>("All", "all", "fuzz", AllFields::list);

void append(void* context, const char* text, std::size_t size) {
    static_cast<std::string*>(context)->append(text, size);
}

struct Fixed {
    detail::DataRegistry registry;
    detail::TypeNode types[2] = {{&kPart, nullptr}, {&kAll, nullptr}};
    detail::EnumNode kind{&kKind, nullptr};
    Schema schema;
    Fixed() {
        add_type(registry, types[0]);
        add_type(registry, types[1]);
        add_enum(registry, kind);
        std::string description;
        detail::write_schema(registry, append, &description);
        std::string error;
        if (!read_schema(description, schema, error)) std::abort();
    }
};

const Fixed& fixed() {
    static const Fixed f;
    return f;
}

/// An accepted conversion must load in the engine, and every reference it reports must be a usable path.
void check_accepted(const ConvertedData& out) {
    const Fixed& f = fixed();
    const std::uint32_t fingerprint = detail::schema_fingerprint(f.registry, kAll);
    if (!detail::validate_data(out.bytes.data(), out.bytes.size(), f.registry, kAll, fingerprint).ok())
        std::abort();
    All all;
    kAll.reset(&all);
    detail::apply_data(out.bytes.data(), out.bytes.size(), f.registry, kAll, &all);
    if (!std::isfinite(all.ratio) || all.ratio > 2 || all.i8 < -50 || all.parts.size() > 3) std::abort();
    for (const DataReference& r : out.references) {
        if (!detail::valid_asset_path(r.path.c_str()) || detail::fnv1a32(r.path.c_str()) == 0 || r.line < 1)
            std::abort();
    }
}

/// The description reader on arbitrary text; a schema it accepts converts `text` against its first keyed
/// type.
void own_schema(std::string_view description, std::string_view text) {
    Schema schema;
    std::string error;
    if (!read_schema(description, schema, error)) {
        if (error.empty()) std::abort();
        return;
    }
    for (const detail::TypeNode* n = schema.registry().types; n != nullptr; n = n->next) {
        if (n->info->key[0] == '\0') continue;
        ConvertedData out;
        Diagnostics problems;
        if (convert_data(schema, *n->info, text, out, problems) &&
            !detail::validate_data(out.bytes.data(), out.bytes.size(), schema.registry(), *n->info,
                                   schema.fingerprint(*n->info))
                 .ok()) {
            std::abort();
        }
        return;
    }
}

} // namespace

const char* data_seed() {
    return "flag = true\ni8 = -50\nu32 = 4_000_000_000\nratio = 1.5e-1\nname = \"\\u00E7a\"\nkind = "
           "\"High\"\n"
           "at = [1, -2.5]\ntint = \"#01020304\"\nsprite = \"art/a\"\nsound = \"sfx/b\"\nmusic = \"\"\n"
           "row = [1, -2, 3]\npath = [[0, 0], [1.5, 2]]\n[part]\nhp = 9\nlink = \"parts/p\"\n"
           "[[parts]]\nhp = 1\n[[parts]]\nhp = 2\nlink = \"parts/q\"\n";
}

bool data_converts(const char* text, std::size_t size) {
    ConvertedData out;
    Diagnostics problems;
    return convert_data(fixed().schema, *fixed().schema.type_by_key("all"), std::string_view(text, size), out,
                        problems);
}

const char* refs_seed() {
    return "nnh-data-refs 1\nasset all 120 things/one\nref 3 data part part.link parts/p\n"
           "ref 9 texture - sprite art/a\nref 10 sound - sound sfx/b\nref 11 music - music mu sic/c\n";
}

int refs_target(const std::uint8_t* data, std::size_t size) {
    RefsFile f;
    std::string error;
    if (!read_refs(std::string_view(reinterpret_cast<const char*>(data), size), f, error)) {
        if (error.rfind("line ", 0) != 0) std::abort(); // every refusal names its line
        return 0;
    }
    IndexInput in;
    in.schema = &fixed().schema;
    for (const DataReference& r : f.references) { // the media it names exist, so the index step can accept it
        in.assets.push_back(r.path + (r.kind == detail::FieldKind::TextureRef ? ".png" : ".wav"));
    }
    in.data.push_back(f);
    in.data.push_back(f); // the same name twice: refused (unless it isn't a data asset)
    IndexModel model;
    std::string errors;
    if (build_index(in, model, errors) == errors.empty()) {
        in.data.pop_back();
        std::string problem;
        if (build_index(in, model, errors) && write_index(model, problem).empty()) std::abort(); // it validates
    } else {
        std::abort(); // a refusal always prints something, an acceptance nothing
    }
    return 0;
}

int data_target(const std::uint8_t* data, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    const std::size_t split = input.find('\0');
    const std::string_view text = input.substr(0, split);
    ConvertedData out;
    Diagnostics problems;
    if (convert_data(fixed().schema, *fixed().schema.type_by_key("all"), text, out, problems)) {
        check_accepted(out);
    } else if (problems.empty() || problems.text("f").find("f:") != 0) {
        std::abort(); // every refusal says where and why
    }
    if (split != std::string_view::npos) own_schema(input.substr(split + 1), text);
    return 0;
}

} // namespace nnh::assetc::fuzz
