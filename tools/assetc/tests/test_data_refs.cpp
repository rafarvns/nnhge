// Reading the references sidecar back (spec 014 T6.1): what `nnh-assetc data` wrote, for the index step.
// Bounded and strict: anything the writer can't produce is refused with its line.

#include <string>

#include "convert_fixture.hpp"
#include "data_refs.hpp"
#include "test.hpp"

using nnh::assetc::read_refs;
using nnh::assetc::RefsFile;
using nnh::detail::FieldKind;

namespace {

std::string problem_of(const std::string& text) {
    RefsFile f;
    std::string error;
    CHECK(!read_refs(text, f, error));
    return error;
}

} // namespace

NNH_TEST(data_refs_reader_reads_back_what_the_writer_wrote) {
    const auto r = convert_test::convert(
        "name = \"Gob\"\nsprite = \"art/gob lin\"\nhurt = \"sfx/hit\"\ntheme = \"music/a\"\n[[drops]]\n"
        "item = \"items/potion\"\n");
    CHECK(r.ok);
    RefsFile f;
    std::string error;
    CHECK(read_refs(nnh::assetc::refs_text("enemies/go blin", "enemy", 123, r.data.references), f, error));
    CHECK(error.empty() && f.data && f.key == "enemy" && f.size == 123 && f.name == "enemies/go blin");
    CHECK(f.references.size() == 4);
    CHECK(f.references[0].line == 2 && f.references[0].kind == FieldKind::TextureRef);
    CHECK(f.references[0].target.empty() && f.references[0].field == "sprite");
    CHECK(f.references[0].path == "art/gob lin");
    // in field order: sprite, hurt, drops, theme
    CHECK(f.references[1].kind == FieldKind::SoundRef && f.references[3].kind == FieldKind::MusicRef);
    CHECK(f.references[2].line == 6 && f.references[2].kind == FieldKind::DataRef);
    CHECK(f.references[2].target == "item" && f.references[2].field == "drops[0].item");
    CHECK(f.references[2].path == "items/potion");
}

NNH_TEST(data_refs_reader_reads_the_sidecar_of_a_copied_file) {
    RefsFile f;
    f.data = true;
    std::string error;
    CHECK(read_refs(nnh::assetc::empty_refs_text(), f, error));
    CHECK(!f.data && f.references.empty());
}

NNH_TEST(data_refs_reader_refuses_what_the_writer_never_writes) {
    CHECK(problem_of("") == "line 1: expected 'nnh-data-refs 1'");
    CHECK(problem_of("nnh-data-refs 2\n") == "line 1: expected 'nnh-data-refs 1'");
    CHECK(problem_of("nnh-data-refs 1") == "line 1: expected 'nnh-data-refs 1'"); // no LF
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a\nasset enemy 12 b\n") == "line 3: a second 'asset' line");
    CHECK(problem_of("nnh-data-refs 1\nref 1 data item f items/a\n") == "line 2: 'ref' before 'asset'");
    CHECK(problem_of("nnh-data-refs 1\nasset Enemy 12 a\n") == "line 2: bad key 'Enemy'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 65537 a\n") == "line 2: bad size '65537'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy x a\n") == "line 2: bad size 'x'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 012 a\n") == "line 2: bad size '012'");
    CHECK(problem_of("nnh-data-refs 1\nasset  enemy 12 a\n") == "line 2: expected 'asset <key> <size> <name>'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12\n") == "line 2: expected 'asset <key> <size> <name>'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a\nref 0 data item f p\n") == "line 3: bad line '0'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a\nref 1 image - f p\n") == "line 3: bad kind 'image'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a\nref 1 data - f p\n") == "line 3: bad key '-'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a\nref 1 texture item f p\n") ==
          "line 3: a texture reference has no type");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a\nref 1 data item f\n") ==
          "line 3: expected 'ref <line> <kind> <type or -> <field> <path>'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a\nlink x\n") == "line 3: unknown record 'link'");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a") == "line 2: the last line has no LF");
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a//b\n") == "line 2: bad asset path 'a//b'");
    CHECK(problem_of(std::string("nnh-data-refs 1\nasset enemy 12 ab\0c\n", 36)).rfind("line 2: bad asset path", 0) == 0);
    CHECK(problem_of("nnh-data-refs 1\nasset enemy 12 a\nref 1 texture - f ../x\n") ==
          "line 3: bad asset path '../x'");
}
