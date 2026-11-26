// `nnh-assetc strings` and `strings-index` (cli_internal.hpp; spec 029 FR-15..FR-21, FR-31): one language's table, then
// the manifest (and the pseudo-language in Debug/Development). Errors print "file:line: reason" and stop the build;
// warnings print and don't, unless --strict. Outputs are rewritten only when they change.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cli_internal.hpp"
#include "files.hpp"
#include "strings_check.hpp"
#include "strings_index.hpp"
#include "strings_read.hpp"

namespace nnh::assetc {
namespace {

struct Args {
    std::string code, languages, out, fonts, pseudo_out;
    bool release = false, strict = false;
};

bool parse(int argc, const char* const* argv, int first, Args& a) {
    for (int i = first; i < argc; ++i) {
        const std::string k = argv[i];
        if (k == "--release") a.release = true;
        else if (k == "--strict") a.strict = true;
        else if (i + 1 >= argc) return false;
        else if (k == "--languages") a.languages = argv[++i];
        else if (k == "--out") a.out = argv[++i];
        else if (k == "--fonts") a.fonts = argv[++i];
        else if (k == "--pseudo-out") a.pseudo_out = argv[++i];
        else return false;
    }
    return !a.languages.empty() && !a.out.empty();
}

/// Reads every language of the list (or only `only` and the base). @return false on a read problem (printed).
bool read_all(FileSource& files, const LanguageList& list, const std::string& only, std::vector<StringsLanguage>& out,
              std::FILE* err) {
    bool ok = true;
    for (std::size_t i = 0; i < list.codes.size(); ++i) {
        StringsLanguage l;
        l.code = list.codes[i];
        if (only.empty() || l.code == only || static_cast<int>(i) == list.base) {
            for (const std::string& f : list.files[i]) {
                std::string text;
                std::vector<std::string> problems;
                if (!files.read(f, kMaxStringsFileBytes + 1, text)) problems.push_back(f + ": can't be read");
                else read_strings(text, f, l, problems);
                for (const std::string& p : problems) std::fprintf(err, "%s\n", p.c_str());
                ok = ok && problems.empty();
            }
        }
        out.push_back(std::move(l));
    }
    return ok;
}

int report(const CheckResult& r, bool strict, std::FILE* out, std::FILE* err) {
    for (const std::string& e : r.errors) std::fprintf(err, "%s\n", e.c_str());
    for (const std::string& w : r.warnings) std::fprintf(strict ? err : out, "%s\n", w.c_str());
    return r.errors.empty() && (!strict || r.warnings.empty()) ? 0 : 1;
}

} // namespace

int run_strings(int argc, const char* const* argv, std::FILE* out, std::FILE* err) {
    Args a;
    if (argc < 3 || !parse(argc, argv, 3, a)) return 2;
    a.code = argv[2];
    DiskFiles files;
    std::string list_text;
    LanguageList list;
    if (!files.read(a.languages, 1u << 20, list_text) || !read_language_list(list_text, list)) {
        std::fprintf(err, "%s: not a language list\n", a.languages.c_str());
        return 1;
    }
    std::vector<StringsLanguage> langs;
    if (!read_all(files, list, a.code, langs, err)) return 1;
    int me = -1;
    for (std::size_t i = 0; i < langs.size(); ++i)
        if (langs[i].code == a.code) me = static_cast<int>(i);
    if (me < 0) return 2;
    CheckResult r;
    check_language(langs[static_cast<std::size_t>(me)], langs[static_cast<std::size_t>(list.base)], r);
    const int code = report(r, a.strict, out, err);
    if (code != 0) {
        std::remove(a.out.c_str());
        return code;
    }
    const std::vector<std::uint8_t> table = write_table(langs[static_cast<std::size_t>(me)], a.release);
    return write_file_if_changed(a.out, table.data(), table.size()) ? 0 : 1;
}

int run_strings_index(int argc, const char* const* argv, std::FILE* out, std::FILE* err) {
    Args a;
    if (!parse(argc, argv, 2, a)) return 2;
    DiskFiles files;
    std::string list_text;
    LanguageList list;
    if (!files.read(a.languages, 1u << 20, list_text) || !read_language_list(list_text, list)) {
        std::fprintf(err, "%s: not a language list\n", a.languages.c_str());
        return 1;
    }
    StringsIndexInput in;
    in.base = list.base;
    in.release = a.release;
    in.fonts_dir = a.fonts;
    if (!read_all(files, list, "", in.languages, err)) return 1;
    if (!a.release && !a.pseudo_out.empty()) in.languages.push_back(make_pseudo(in.languages[static_cast<std::size_t>(list.base)]));
    std::vector<std::uint8_t> manifest;
    CheckResult r;
    build_index(files, in, manifest, r);
    const int code = report(r, a.strict, out, err);
    if (code != 0) {
        std::remove(a.out.c_str());
        return code;
    }
    if (!a.release && !a.pseudo_out.empty()) {
        const std::vector<std::uint8_t> q = write_table(in.languages.back(), false);
        if (!write_file_if_changed(a.pseudo_out, q.data(), q.size())) return 1;
    }
    return write_file_if_changed(a.out, manifest.data(), manifest.size()) ? 0 : 1;
}

} // namespace nnh::assetc
