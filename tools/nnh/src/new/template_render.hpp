#pragma once
// The files of a new project (spec 033 FR-7, FR-8, FR-9): the embedded template with its placeholders replaced. Only
// ".in" files are rendered; every other file is copied as embedded. A placeholder is `@word@` (lowercase letters and
// '_'); an unknown one is an error (a template bug, caught by the tests), and values are inserted once, never scanned
// again. Pure and deterministic; tested in tools/nnh/tests/test_template_render.cpp.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "new/template_files.hpp"

namespace nnh::tool {

struct NewInputs {
    std::string name;           ///< checked by check_name
    std::string title;          ///< checked by check_text_field
    std::string author;         ///< "" = not given: the AUTHOR keyword is written as a comment
    std::uint32_t unique_id = 0;
    std::string engine_url;     ///< the pin ("" = none: the guard)
    std::string engine_sha256;
    std::string engine_version; ///< this nnh's version: nnh.toml's `engine`, the minimum checked by CMakeLists.txt
};

struct OutputFile {
    std::string path;  ///< relative to the project folder, '/'-separated
    std::string bytes;
};

struct RenderResult {
    bool ok = false;
    std::string error; ///< "<stored file>: unknown placeholder @x@"
    std::vector<OutputFile> files;
};

/// Renders `count` template files. Placeholders: @name@ @title@ @author_line@ @unique_id@ @engine_url@
/// @engine_sha256@ @engine_version@ @engine_min@ (major.minor) @docs_base@ @engine_state@ (README text).
RenderResult render_files(const TemplateFile* files, std::size_t count, const NewInputs& inputs);

/// render_files over the template nnh embeds.
RenderResult render_project(const NewInputs& inputs);

/// The docs link base a project's README uses: the release's tag for a pinned project, else the main branch.
std::string docs_base(const NewInputs& inputs);

} // namespace nnh::tool
