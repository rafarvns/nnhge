// nnh.toml (project_file.hpp, spec 032 FR-12): name and engine, unknown keys warned, errors with their line.

#include <string>

#include "project_file.hpp"
#include "test.hpp"

using namespace nnh::tool;

NNH_TEST(an_empty_project_file_is_valid) {
    const ProjectFile f = read_project_file("");
    CHECK(f.errors.empty());
    CHECK(f.warnings.empty());
    CHECK(f.name.empty());
    CHECK(f.engine.empty());
    CHECK(read_project_file("# only a comment\n\n").errors.empty());
}

NNH_TEST(name_and_engine_are_read) {
    const ProjectFile f = read_project_file("name = \"mygame\"\nengine = \"1.0.0\"  # made for\n");
    CHECK(f.errors.empty());
    CHECK(f.warnings.empty());
    CHECK(f.name == "mygame");
    CHECK(f.engine == "1.0.0");
}

NNH_TEST(an_unknown_key_is_one_warning_line) {
    const ProjectFile f = read_project_file("name = \"g\"\nicon = \"x.png\"\n");
    CHECK(f.errors.empty());
    CHECK(f.name == "g");
    CHECK(f.warnings.size() == 1);
    CHECK(f.warnings.size() == 1 && f.warnings[0] == "nnh.toml:2: unknown key \"icon\" (ignored)");
}

NNH_TEST(a_key_in_a_table_is_a_warning) {
    const ProjectFile f = read_project_file("[build]\nname = \"g\"\n");
    CHECK(f.errors.empty());
    CHECK(f.name.empty()); // build/name is not the project's name
    CHECK(f.warnings.size() == 1);
    CHECK(f.warnings.size() == 1 && f.warnings[0] == "nnh.toml:2: unknown key \"build/name\" (ignored)");
}

NNH_TEST(malformed_lines_are_errors_with_their_line) {
    const ProjectFile f = read_project_file("name = \"g\"\nthis is not toml\n");
    CHECK(f.errors.size() == 1);
    CHECK(f.errors.size() == 1 && f.errors[0] == "nnh.toml:2: expected \"name = value\"");
    CHECK(read_project_file("[oops\n").errors.size() == 1);
    CHECK(read_project_file("name =\n").errors.size() == 1);
    const ProjectFile twice = read_project_file("name = \"a\"\nname = \"b\"\n");
    CHECK(twice.errors.size() == 1);
}

NNH_TEST(a_project_file_over_64_kib_is_refused) {
    const std::string big(64 * 1024 + 1, '#');
    const ProjectFile f = read_project_file(big);
    CHECK(f.errors.size() == 1);
    CHECK(f.errors.size() == 1 && f.errors[0] == "nnh.toml: the file is larger than 64 KiB");
    CHECK(read_project_file(std::string(64 * 1024, '#')).errors.empty());
}
