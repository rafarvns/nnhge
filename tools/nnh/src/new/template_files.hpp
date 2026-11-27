#pragma once
// The project template embedded in nnh (spec 033 FR-7, D56 item 1): the files of tools/nnh/template/, turned into one
// generated source by tools/nnh/embed_template.cmake at build time (text with LF line endings, a fixed order).

#include <cstddef>

namespace nnh::tool {

struct TemplateFile {
    const char* stored;         ///< its path in tools/nnh/template/ ("CMakeLists.txt.in", "gitignore", "src/main.cpp")
    const char* written;        ///< its path in a new project ("CMakeLists.txt", ".gitignore", "src/main.cpp")
    const unsigned char* bytes; ///< the content (`size` bytes)
    std::size_t size;
    bool text;   ///< text (LF line endings) or binary (an image, a sound)
    bool render; ///< a ".in" file: placeholders (@name@) are replaced
};

extern const TemplateFile kProjectTemplate[]; ///< the template nnh new writes (generated template_files.cpp)
extern const std::size_t kProjectTemplate_count;

} // namespace nnh::tool
