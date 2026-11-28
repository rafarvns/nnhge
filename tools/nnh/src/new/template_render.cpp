// The files of a new project (template_render.hpp).

#include "new/template_render.hpp"

#include "new/project_name.hpp"

#include <utility>

namespace nnh::tool {
namespace {

constexpr const char* kRepository = "https://github.com/rafarvns/nnhge";

bool placeholder_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/// "0.1" of "0.1.0".
std::string major_minor(const std::string& version) {
    const std::size_t first = version.find('.');
    const std::size_t second = first == std::string::npos ? first : version.find('.', first + 1);
    return second == std::string::npos ? version : version.substr(0, second);
}

std::string author_line(const NewInputs& in) {
    if (in.author.empty())
        return "    # AUTHOR \"Your name\"               # shown in the HOME Menu (default: \"Unknown\")";
    return "    AUTHOR \"" + in.author + "\"";
}

/// README text about the engine this project uses (its section "The engine").
std::string engine_state(const NewInputs& in) {
    if (!in.engine_url.empty()) {
        return "This project pins NNHGE " + in.engine_version +
               ": `CMakeLists.txt` downloads that release's archive "
               "and checks its SHA-256, so every build uses the same engine.";
    }
    return "No engine release is pinned yet: this project was made by an `nnh` built from an engine "
           "checkout, so it "
           "builds with a local engine. Set `NNHGE_ENGINE_DIR` to that checkout (below) before the first "
           "build; "
           "`nnh new` printed the lines for your shells.";
}

/// The value of placeholder `word`; false when there is no such placeholder.
bool value_of(const std::string& word, const NewInputs& in, std::string& out) {
    const std::pair<const char*, std::string> values[] = {
        {"name", in.name},
        {"title", in.title},
        {"author_line", author_line(in)},
        {"unique_id", format_unique_id(in.unique_id)},
        {"engine_url", in.engine_url},
        {"engine_sha256", in.engine_sha256},
        {"engine_version", in.engine_version},
        {"engine_min", major_minor(in.engine_version)},
        {"docs_base", docs_base(in)},
        {"engine_state", engine_state(in)},
    };
    for (const auto& [key, value] : values) {
        if (word == key) {
            out = value;
            return true;
        }
    }
    return false;
}

/// `text` with every @word@ replaced once; false with `error` set for an unknown placeholder.
bool render_text(const std::string& text, const NewInputs& in, std::string& out, std::string& error) {
    out.clear();
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '@') {
            out += text[i++];
            continue;
        }
        std::size_t end = i + 1;
        while (end < text.size() && placeholder_char(text[end]))
            ++end;
        if (end == i + 1 || end >= text.size() || text[end] != '@') { // not a placeholder: '@' as written
            out += text[i++];
            continue;
        }
        const std::string word = text.substr(i + 1, end - i - 1);
        std::string value;
        if (!value_of(word, in, value)) {
            error = "unknown placeholder @" + word + "@";
            return false;
        }
        out += value;
        i = end + 1;
    }
    return true;
}

} // namespace

RenderResult render_files(const TemplateFile* files, std::size_t count, const NewInputs& inputs) {
    RenderResult r;
    for (std::size_t k = 0; k < count; ++k) {
        const TemplateFile& f = files[k];
        OutputFile out{f.written, std::string(reinterpret_cast<const char*>(f.bytes), f.size)};
        if (f.render) {
            std::string rendered, error;
            if (!render_text(out.bytes, inputs, rendered, error)) {
                r.error = std::string(f.stored) + ": " + error;
                r.files.clear();
                return r;
            }
            out.bytes = std::move(rendered);
        }
        r.files.push_back(std::move(out));
    }
    r.ok = true;
    return r;
}

RenderResult render_project(const NewInputs& inputs) {
    return render_files(kProjectTemplate, kProjectTemplate_count, inputs);
}

std::string docs_base(const NewInputs& inputs) {
    const std::string ref = inputs.engine_url.empty() ? "main" : "v" + inputs.engine_version;
    return std::string(kRepository) + "/blob/" + ref + "/docs";
}

} // namespace nnh::tool
