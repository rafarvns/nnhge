// Mini-fuzz of nnh-assetc's untrusted inputs (spec 012 T5.1, T5.8): seeded, deterministic mutations of valid
// documents run through the fuzz target bodies (fuzz_assetc.cpp), which abort on a broken invariant. The libFuzzer
// programs (fuzz_assetc_json, fuzz_assetc_map) run the same bodies for longer on Linux.

#include <cstdint>
#include <string>

#include "convert.hpp"
#include "data/schema_write.hpp"
#include "files.hpp"
#include "fuzz_assetc.hpp"
#include "json.hpp"
#include "test.hpp"
#include "toml.hpp"

namespace {

struct Rng {
    std::uint32_t state;
    std::uint32_t next() { return state = state * 1664525u + 1013904223u; }
};

/// 1-6 edits: replace, delete, insert a byte, or insert a JSON-significant character.
void mutate(std::string& s, Rng& rng) {
    static const char kSignificant[] = "{}[]\",:\\-.0123456789eEtfnu \n";
    const int edits = 1 + static_cast<int>(rng.next() % 6);
    for (int e = 0; e < edits; ++e) {
        const std::uint32_t r = rng.next();
        const std::size_t at = s.empty() ? 0 : (r >> 8) % s.size();
        const char byte = static_cast<char>(rng.next() >> 16);
        switch (r % 4) {
        case 0: if (!s.empty()) s[at] = byte; break;
        case 1: if (!s.empty()) s.erase(at, 1); break;
        case 2: s.insert(at, 1, byte); break;
        default: s.insert(at, 1, kSignificant[(rng.next() >> 8) % (sizeof kSignificant - 1)]); break;
        }
    }
}

const char* const kJsonSeeds[] = {
    R"({"type":"map","width":4,"layers":[{"data":[1,2,3,2147483649],"name":"aç"}],"x":-1.5e3,"ok":true})",
    R"([null,false,[[]],{"k":{"k":"😀"}},0.25,"tab\tquote\""])",
};

// A map using every feature the converter reads, and its tileset (the fuzz input is map '\0' tileset).
const char kMapSeed[] = R"({"type":"map","orientation":"orthogonal","infinite":false,"width":3,"height":2,
"tilewidth":16,"tileheight":16,"parallaxoriginx":4,"properties":[{"name":"music","value":"a"},
{"name":"g","type":"float","value":1.5}],"tilesets":[{"firstgid":1,"source":"../tiles/set.tsj"},
{"firstgid":20,"image":"../tiles/set.png","imagewidth":64,"imageheight":64,"tilewidth":16,"tileheight":16,
"tilecount":16,"columns":4}],"layers":[{"type":"tilelayer","name":"a","width":3,"height":2,"opacity":0.5,
"parallaxx":0.5,"data":[1,2147483650,536870915,20,0,35],"properties":[{"name":"collision","type":"bool","value":true}]},
{"type":"tilelayer","name":"b","encoding":"base64","data":"AQAAAAIAAAAAAAAAAAAAAAAAAAAUAACA"},
{"type":"objectgroup","name":"o","objects":[{"id":1,"name":"spawn","x":1,"y":2,"point":true},
{"id":2,"type":"coin","x":3,"y":4,"width":16,"height":16,"properties":[{"name":"v","type":"int","value":5}]}]}]})";
const char kSetSeed[] = R"({"image":"set.png","imagewidth":64,"imageheight":64,"tilewidth":16,"tileheight":16,
"tilecount":16,"columns":4,"tiles":[{"id":1,"properties":[{"name":"solid","type":"bool","value":true}],
"animation":[{"tileid":1,"duration":100},{"tileid":5,"duration":50}]},
{"id":2,"properties":[{"name":"one_way","type":"bool","value":true},{"name":"k","value":"x"}]},
{"id":9,"animation":[{"tileid":15,"duration":7}]}]})";

// Data files (spec 014): every form the TOML-subset reader accepts.
const char kTomlSeed[] = "\xEF\xBB\xBF# a goblin\r\nname = \"Gob\\u00E7lin \\\"g\\\"\\n\"\r\nhp = 1_000\n"
                         "speed = -2.5e-1 # fast\nflags = [true, false,\n  true, ] \npath = [[1, 2], [3.5, -4]]\n"
                         "[stats]\nattack = +3\n[stats.extra]\nx = 0\n[[drops]]\nitem = \"items/potion\"\n"
                         "chance = 0.25\n[[drops]]\n[drops.more]\ny = 1\n";

/// 1-6 edits, inserting TOML-significant characters as well.
void mutate_toml(std::string& s, Rng& rng) {
    static const char kSignificant[] = "[]\"'=#.,{}_+-eE0123456789\\u \t\r\n";
    const int edits = 1 + static_cast<int>(rng.next() % 6);
    for (int e = 0; e < edits; ++e) {
        const std::uint32_t r = rng.next();
        const std::size_t at = s.empty() ? 0 : (r >> 8) % s.size();
        switch (r % 4) {
        case 0: if (!s.empty()) s[at] = static_cast<char>(rng.next() >> 16); break;
        case 1: if (!s.empty()) s.erase(at, 1); break;
        case 2: s.insert(at, 1, static_cast<char>(rng.next() >> 16)); break;
        default: s.insert(at, 1, kSignificant[(rng.next() >> 8) % (sizeof kSignificant - 1)]); break;
        }
    }
}

bool converts(const std::string& map, const std::string& set) {
    nnh::assetc::MemoryFiles files;
    files.add("/g/assets/levels/map.tmj", map);
    files.add("/g/assets/tiles/set.tsj", set);
    files.add("/g/assets/tiles/set.png", std::string("\x89PNG\r\n\x1a\n\0\0\0\x0DIHDR\0\0\0\x40\0\0\0\x40", 24));
    nnh::assetc::Converted out;
    nnh::assetc::Problem problem;
    return nnh::assetc::convert_map(files, "/g/assets/levels/map.tmj", "/g/assets", out, problem);
}

} // namespace

NNH_TEST(assetc_json_target_survives_mutated_documents) {
    Rng rng{12345};
    int accepted = 0;
    for (const char* seed : kJsonSeeds) {
        for (int i = 0; i < 4000; ++i) {
            std::string doc = seed;
            mutate(doc, rng);
            nnh::assetc::fuzz::json_target(reinterpret_cast<const std::uint8_t*>(doc.data()), doc.size());
            nnh::assetc::json::Document d;
            accepted += d.parse(doc) ? 1 : 0;
        }
    }
    CHECK(accepted > 50);   // some mutants stay valid, so the walk runs
    CHECK(accepted < 7000); // and most are refused
}

NNH_TEST(assetc_map_target_seed_converts) {
    CHECK(converts(kMapSeed, kSetSeed));
}

NNH_TEST(assetc_map_target_survives_mutated_maps_and_tilesets) {
    Rng rng{777};
    int accepted = 0;
    for (int i = 0; i < 6000; ++i) {
        std::string map = kMapSeed, set = kSetSeed;
        mutate(i % 2 == 0 ? map : set, rng);
        const std::string input = map + std::string(1, '\0') + set;
        nnh::assetc::fuzz::map_target(reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
        accepted += converts(map, set) ? 1 : 0;
    }
    CHECK(accepted > 100);  // the deep path (writer + validator) runs
    CHECK(accepted < 5500); // and refusals too
}

NNH_TEST(assetc_toml_target_seed_parses) {
    nnh::assetc::toml::Document d;
    CHECK(d.parse(kTomlSeed));
    CHECK(d.root().find("drops") != nullptr && d.root().find("drops")->elements.size() == 2);
}

NNH_TEST(assetc_toml_target_survives_mutated_documents) {
    Rng rng{2024};
    int accepted = 0;
    for (int i = 0; i < 8000; ++i) {
        std::string doc = kTomlSeed;
        mutate_toml(doc, rng);
        nnh::assetc::fuzz::toml_target(reinterpret_cast<const std::uint8_t*>(doc.data()), doc.size());
        nnh::assetc::toml::Document d;
        accepted += d.parse(doc) ? 1 : 0;
    }
    CHECK(accepted > 200);  // many mutants stay valid, so the walk runs
    CHECK(accepted < 7600); // and refusals too
    std::string big(70 * 1024, '#');
    nnh::assetc::fuzz::toml_target(reinterpret_cast<const std::uint8_t*>(big.data()), big.size());
}

NNH_TEST(assetc_data_target_seed_converts) {
    // Every field set: the deep path (validator + apply) runs on the seed itself.
    const std::string seed = nnh::assetc::fuzz::data_seed();
    nnh::assetc::fuzz::data_target(reinterpret_cast<const std::uint8_t*>(seed.data()), seed.size());
    CHECK(nnh::assetc::fuzz::data_converts(seed.data(), seed.size()));
}

NNH_TEST(assetc_data_target_survives_mutated_files_and_schemas) {
    Rng rng{99};
    std::string description;
    nnh::detail::write_schema(
        nnh::detail::data_registry(),
        [](void* c, const char* t, std::size_t n) { static_cast<std::string*>(c)->append(t, n); }, &description);
    const std::string enemy = "name = \"Gob\"\nlevel = 3\nspawn = [1, 2]\n[stats]\nhp = 4\n[[drops]]\nitem = \"i/p\"\n";
    int accepted = 0;
    for (int i = 0; i < 6000; ++i) {
        std::string doc = nnh::assetc::fuzz::data_seed();
        mutate_toml(doc, rng);
        accepted += nnh::assetc::fuzz::data_converts(doc.data(), doc.size()) ? 1 : 0;
        if (i % 4 == 0) {
            std::string schema = description; // the tests' schema, with the enemy as the data file
            if (i % 8 == 0) mutate_toml(schema, rng);
            doc = enemy + std::string(1, '\0') + schema;
        }
        nnh::assetc::fuzz::data_target(reinterpret_cast<const std::uint8_t*>(doc.data()), doc.size());
    }
    CHECK(accepted > 300);  // the deep path (writer, engine validator, apply) runs
    CHECK(accepted < 5700); // and refusals too
}
