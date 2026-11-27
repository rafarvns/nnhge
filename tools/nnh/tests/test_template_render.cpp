// The embedded template and its rendering (new/template_files.hpp, new/template_render.hpp; spec 033 FR-7 to FR-9).

#include <fstream>
#include <iterator>
#include <string>

#include "new/template_render.hpp"
#include "test.hpp"

using namespace nnh::tool;

namespace nnh::tool {
extern const TemplateFile kStandinTemplate[]; // tests/data/template_standin, embedded like the real one
extern const std::size_t kStandinTemplate_count;
} // namespace nnh::tool

namespace {

std::string file_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string bytes_of(const TemplateFile& f) { return std::string(reinterpret_cast<const char*>(f.bytes), f.size); }

NewInputs inputs() {
    NewInputs in;
    in.name = "demo";
    in.title = "Demo Game";
    in.unique_id = 0x5A3C1;
    in.engine_version = "0.1.0";
    return in;
}

const TemplateFile* stand_in(const char* written) {
    for (std::size_t i = 0; i < kStandinTemplate_count; ++i) {
        if (std::string(kStandinTemplate[i].written) == written) return &kStandinTemplate[i];
    }
    return nullptr;
}

TemplateFile text_file(const char* stored, const char* written, const char* content) {
    return {stored, written, reinterpret_cast<const unsigned char*>(content), std::string(content).size(), true, true};
}

std::string render_one(const char* content, const NewInputs& in, bool* ok = nullptr, std::string* error = nullptr) {
    const TemplateFile f = text_file("x.in", "x", content);
    const RenderResult r = render_files(&f, 1, in);
    if (ok) *ok = r.ok;
    if (error) *error = r.error;
    return r.ok ? r.files[0].bytes : "";
}

} // namespace

NNH_TEST(the_embed_step_keeps_order_names_and_bytes) {
    CHECK(kStandinTemplate_count == 4);
    CHECK(std::string(kStandinTemplate[0].stored) == "greeting.txt.in");
    CHECK(std::string(kStandinTemplate[1].stored) == "art/blob.png");
    CHECK(std::string(kStandinTemplate[2].stored) == "gitignore");
    CHECK(std::string(kStandinTemplate[3].stored) == "notes.txt");
    const TemplateFile* greeting = stand_in("greeting.txt");
    CHECK(greeting && greeting->text && greeting->render);
    CHECK(greeting && bytes_of(*greeting) == "Hello @name@\nbye\n"); // CRLF on disk, LF embedded
    const TemplateFile* blob = stand_in("art/blob.png");
    CHECK(blob && !blob->text && !blob->render);
    const std::string original = file_bytes(NNH_TOOL_SOURCE_DIR "/tests/data/template_standin/art/blob.png");
    CHECK(!original.empty() && blob && bytes_of(*blob) == original); // binary: byte for byte, CR LF and NUL kept
    CHECK(stand_in(".gitignore") != nullptr && !stand_in(".gitignore")->render);
    CHECK(stand_in("notes.txt") && !stand_in("notes.txt")->render);
}

NNH_TEST(only_in_files_are_rendered_and_binaries_are_copied) {
    const RenderResult r = render_files(kStandinTemplate, kStandinTemplate_count, inputs());
    CHECK(r.ok && r.files.size() == 4);
    if (r.files.size() != 4) return;
    CHECK(r.files[0].path == "greeting.txt" && r.files[0].bytes == "Hello demo\nbye\n");
    CHECK(r.files[1].path == "art/blob.png" && r.files[1].bytes == bytes_of(kStandinTemplate[1]));
    CHECK(r.files[3].path == "notes.txt" && r.files[3].bytes.find("@name@") != std::string::npos); // not rendered
}

NNH_TEST(placeholders_are_replaced_once) {
    NewInputs in = inputs();
    CHECK(render_one("@name@|@title@|@unique_id@|@engine_version@|@engine_min@", in) ==
          "demo|Demo Game|0x5A3C1|0.1.0|0.1");
    in.engine_version = "12.34.5";
    CHECK(render_one("@engine_min@", in) == "12.34");
    in.title = "@name@"; // a value is never scanned again (titles can't hold '@', but URLs can)
    CHECK(render_one("@title@", in) == "@name@");
    CHECK(render_one("a@b.c @ x@y", in) == "a@b.c @ x@y"); // not placeholders: copied
    CHECK(render_one("@@name@", in) == "@demo");
}

NNH_TEST(an_unknown_placeholder_is_an_error) {
    bool ok = true;
    std::string error;
    render_one("x @nmae@ y", inputs(), &ok, &error);
    CHECK(!ok && error.find("x.in") != std::string::npos && error.find("@nmae@") != std::string::npos);
}

NNH_TEST(the_author_line_is_a_keyword_or_a_comment) {
    NewInputs in = inputs();
    const std::string none = render_one("@author_line@", in);
    CHECK(none.find("# AUTHOR \"Your name\"") != std::string::npos);
    in.author = "Me";
    const std::string given = render_one("@author_line@", in);
    CHECK(given.find("AUTHOR \"Me\"") != std::string::npos && given.find("# AUTHOR") == std::string::npos);
}

NNH_TEST(the_pin_and_the_docs_follow_the_build_kind) {
    NewInputs in = inputs();
    CHECK(render_one("[@engine_url@][@engine_sha256@]", in) == "[][]");
    CHECK(docs_base(in) == "https://github.com/rafarvns/nnhge/blob/main/docs");
    CHECK(render_one("@engine_state@", in).find("NNHGE_ENGINE_DIR") != std::string::npos);
    in.engine_url = "https://github.com/rafarvns/nnhge/archive/refs/tags/v0.1.0.tar.gz";
    in.engine_sha256 = std::string(64, 'b');
    CHECK(render_one("[@engine_url@][@engine_sha256@]", in) == "[" + in.engine_url + "][" + in.engine_sha256 + "]");
    CHECK(docs_base(in) == "https://github.com/rafarvns/nnhge/blob/v0.1.0/docs");
    CHECK(render_one("@engine_state@", in).find("0.1.0") != std::string::npos);
}

NNH_TEST(rendering_is_deterministic) {
    const RenderResult a = render_files(kStandinTemplate, kStandinTemplate_count, inputs());
    const RenderResult b = render_files(kStandinTemplate, kStandinTemplate_count, inputs());
    CHECK(a.ok && b.ok && a.files.size() == b.files.size());
    for (std::size_t i = 0; i < a.files.size() && i < b.files.size(); ++i) {
        CHECK(a.files[i].path == b.files[i].path && a.files[i].bytes == b.files[i].bytes);
    }
}
