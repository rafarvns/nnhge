// nnh new <name> (spec 033 FR-3 to FR-11, D56): validate the values, choose the engine, render the embedded template,
// write it all or nothing, then say what was made and what to run next. Runs no other program and needs no network.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "build_info.hpp"
#include "commands.hpp"
#include "new/engine_location.hpp"
#include "new/project_name.hpp"
#include "new/project_writer.hpp"
#include "new/template_render.hpp"
#include "new/unique_id.hpp"

namespace nnh::tool {
namespace {

int refuse(const std::string& why) {
    std::fprintf(stderr, "nnh: %s\n", why.c_str());
    return 1;
}

/// The current folder in generic UTF-8 form ("E:/games").
std::string current_folder() {
    std::error_code ec;
    const std::u8string u8 = std::filesystem::current_path(ec).generic_u8string();
    return std::string(u8.begin(), u8.end());
}

/// The checked values of the command line, or a refusal (exit 1).
bool new_inputs(const Options& options, NewInputs& in, std::string& error) {
    const NameCheck name = check_name(options.game);
    if (!name.ok) {
        error = "\"" + options.game + "\" can't be a game name: " + name.rule +
                (name.suggestion.empty() ? "" : " (try " + name.suggestion + ")");
        return false;
    }
    in.name = options.game;
    in.title = options.has_title ? options.title : options.game;
    if (const TextCheck t = check_text_field(in.title, false); !t.ok) {
        error = "--title \"" + in.title + "\": " + t.rule;
        return false;
    }
    in.author = options.author;
    if (const TextCheck a = check_text_field(in.author, true); !a.ok) {
        error = "--author \"" + in.author + "\": " + a.rule;
        return false;
    }
    if (options.unique_id.empty()) {
        in.unique_id = random_unique_id();
    } else if (const auto id = parse_unique_id(options.unique_id, error)) {
        in.unique_id = *id;
    } else {
        return false;
    }
    in.engine_version = build_info().version;
    return true;
}

} // namespace

int command_new(const Options& options) {
    NewInputs in;
    std::string error;
    if (!new_inputs(options, in, error)) return refuse(error);

    const BuildInfo& info = build_info();
    const bool windows = info.os == "windows";
    const std::string engine = options.engine.empty() ? "" : engine_dir_from_option(options.engine, current_folder(), windows);
    const EngineChoice choice = choose_engine(info, engine, std::getenv("NNHGE_ENGINE_DIR"), windows, engine_folder_version);
    if (!choice.ok) return refuse(choice.error + ": nothing was created");
    in.engine_url = choice.engine_url;
    in.engine_sha256 = choice.engine_sha256;

    const RenderResult rendered = render_project(in);
    if (!rendered.ok) return refuse("this nnh's template is broken (" + rendered.error + "): please report it");
    const WriteResult written = write_project(in.name, rendered.files, system_new_fs());
    if (!written.ok) return refuse(written.error);

    std::printf("nnh: created %s/ (%zu files; CIA UNIQUE_ID %s)\n", in.name.c_str(), rendered.files.size(),
                format_unique_id(in.unique_id).c_str());
    for (const std::string& line : choice.notes) std::printf("%s\n", line.c_str());
    std::printf("next:\n  cd %s\n  nnh run %s --pc\n  nnh run %s              (Homebrew Launcher open, press Y)\n",
                in.name.c_str(), in.name.c_str(), in.name.c_str());
    return 0;
}

} // namespace nnh::tool
