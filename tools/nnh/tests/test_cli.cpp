// nnh's command line (cli.hpp, spec 010).

#include "cli.hpp"
#include "test.hpp"

using namespace nnh::tool;

namespace {

bool parse(std::vector<std::string> args, Options& o, std::string& e) { return parse_command_line(args, o, e); }

} // namespace

NNH_TEST(cli_parses_commands_games_and_options) {
    Options o;
    std::string e;
    CHECK(parse({"run", "hello", "--console", "192.168.10.52"}, o, e));
    CHECK(o.command == Command::Run && o.game == "hello" && o.console == "192.168.10.52" && !o.release);
    CHECK(parse({"build", "--release", "audio"}, o, e));
    CHECK(o.command == Command::Build && o.game == "audio" && o.release && !o.pc);
    CHECK(parse({"run", "audio", "--pc"}, o, e) && o.pc);
    CHECK(parse({"install", "audio"}, o, e) && o.command == Command::Install);
    CHECK(parse({}, o, e) && o.command == Command::Help);
    CHECK(parse({"help"}, o, e) && o.command == Command::Help);
    CHECK(parse({"--help"}, o, e) && o.command == Command::Help);
}

NNH_TEST(cli_parses_serve_and_unpair) {
    Options o;
    std::string e;
    CHECK(parse({"serve"}, o, e) && o.command == Command::Serve && o.game.empty() && !o.release);
    CHECK(parse({"serve", "--release"}, o, e) && o.command == Command::Serve && o.release);
    CHECK(parse({"unpair"}, o, e) && o.command == Command::Unpair);
    CHECK(!parse({"serve", "hello"}, o, e) && e.find("serve") != std::string::npos); // the console picks games
    CHECK(!parse({"serve", "--pc"}, o, e));
    CHECK(!parse({"serve", "--console", "1.2.3.4"}, o, e));
    CHECK(!parse({"unpair", "--release"}, o, e));
    CHECK(std::string(help_text()).find("nnh serve") != std::string::npos);
}

NNH_TEST(cli_parses_scripts_and_no_build_for_run) {
    Options o;
    std::string e;
    CHECK(parse({"run", "input", "--pc", "--script", "examples/input/tests/remote.nnhs", "--no-build"}, o, e));
    CHECK(o.command == Command::Run && o.game == "input" && o.pc && o.no_build);
    CHECK(o.script == "examples/input/tests/remote.nnhs");
    CHECK(parse({"run", "input"}, o, e) && o.script.empty() && !o.no_build);
    CHECK(parse({"run", "--no-build", "input"}, o, e) && o.no_build && o.script.empty());
    const std::string help = help_text();
    CHECK(help.find("--script <file.nnhs>") != std::string::npos && help.find("--no-build") != std::string::npos);
}

NNH_TEST(a_script_closes_the_game_at_its_end_unless_kept_open) {
    Options o;
    std::string e;
    CHECK(parse({"run", "input", "--pc", "--script", "a.nnhs"}, o, e) && closes_game_at_end(o));
    CHECK(parse({"run", "input", "--script", "a.nnhs"}, o, e) && closes_game_at_end(o)); // back to the launcher
    CHECK(parse({"run", "input", "--pc"}, o, e) && !closes_game_at_end(o)); // typed `end` only detaches
    CHECK(parse({"run", "input"}, o, e) && !closes_game_at_end(o));
    CHECK(parse({"run", "input", "--script", "a.nnhs", "--keep-open"}, o, e) && o.keep_open && !closes_game_at_end(o));
    CHECK(parse({"run", "input", "--keep-open", "--pc", "--script", "a.nnhs"}, o, e) && !closes_game_at_end(o));
    CHECK(!parse({"build", "input", "--keep-open"}, o, e) && e.find("--keep-open") != std::string::npos);
    CHECK(std::string(help_text()).find("--keep-open") != std::string::npos);
}

NNH_TEST(cli_refuses_scripts_where_they_cannot_run) {
    Options o;
    std::string e;
    CHECK(!parse({"run", "input", "--script"}, o, e) && e.find("--script") != std::string::npos);
    CHECK(!parse({"run", "input", "--script", "a.nnhs", "--release"}, o, e));
    CHECK(e.find("Release builds have no remote control") != std::string::npos);
    CHECK(!parse({"run", "input", "--release", "--script", "a.nnhs"}, o, e)); // in either order
    CHECK(!parse({"build", "input", "--script", "a.nnhs"}, o, e) && e.find("--script") != std::string::npos);
    CHECK(!parse({"install", "input", "--no-build"}, o, e) && e.find("--no-build") != std::string::npos);
    CHECK(!parse({"build", "input", "--no-build"}, o, e));
    CHECK(parse({"run", "input", "--release", "--no-build"}, o, e)); // a plain run of a Release build is fine
}

NNH_TEST(cli_reports_mistakes) {
    Options o;
    std::string e;
    CHECK(!parse({"launch", "hello"}, o, e) && e.find("launch") != std::string::npos);
    CHECK(!parse({"run"}, o, e) && e.find("game") != std::string::npos);
    CHECK(!parse({"run", "hello", "--console"}, o, e) && e.find("--console") != std::string::npos);
    CHECK(!parse({"run", "hello", "--fast"}, o, e) && e.find("--fast") != std::string::npos);
    CHECK(!parse({"run", "a", "b"}, o, e));
    CHECK(!parse({"install", "audio", "--pc"}, o, e)); // a .cia is for the console
    CHECK(std::string(help_text()).find("nnh run") != std::string::npos);
}

// ---- spec 033: nnh new, nnh doctor, nnh --version ----

NNH_TEST(cli_parses_version) {
    Options o;
    std::string e;
    CHECK(parse({"--version"}, o, e) && o.command == Command::Version);
    CHECK(!parse({"--version", "hello"}, o, e) && e.find("--version") != std::string::npos);
    CHECK(std::string(help_text()).find("nnh --version") != std::string::npos);
}

NNH_TEST(cli_parses_new_and_its_options) {
    Options o;
    std::string e;
    CHECK(parse({"new", "my-game"}, o, e) && o.command == Command::New && o.game == "my-game");
    CHECK(!o.has_title && o.author.empty() && o.engine.empty() && o.unique_id.empty());
    CHECK(parse({"new", "--title", "My Game", "demo", "--author", "Me", "--engine", "E:/src/nnhge", "--unique-id",
                 "0x5A3C1"}, o, e));
    CHECK(o.game == "demo" && o.has_title && o.title == "My Game" && o.author == "Me");
    CHECK(o.engine == "E:/src/nnhge" && o.unique_id == "0x5A3C1");
    CHECK(parse({"new", "demo", "--title", ""}, o, e) && o.has_title && o.title.empty()); // checked by nnh new itself
    const std::string help = help_text();
    CHECK(help.find("nnh new <name>") != std::string::npos && help.find("--unique-id") != std::string::npos);
    CHECK(help.find("--engine <path>") != std::string::npos && help.find("--author") != std::string::npos);
}

NNH_TEST(cli_refuses_mistakes_in_new) {
    Options o;
    std::string e;
    CHECK(!parse({"new"}, o, e) && e.find("nnh new") != std::string::npos);
    CHECK(!parse({"new", "a", "b"}, o, e));
    CHECK(!parse({"new", "demo", "--pc"}, o, e) && e.find("--pc") != std::string::npos);
    CHECK(!parse({"new", "demo", "--release"}, o, e) && e.find("--release") != std::string::npos);
    CHECK(!parse({"new", "demo", "--fast"}, o, e) && e.find("--fast") != std::string::npos);
    CHECK(!parse({"new", "demo", "--title"}, o, e) && e.find("--title") != std::string::npos);
    CHECK(!parse({"new", "demo", "--author"}, o, e) && e.find("--author") != std::string::npos);
    CHECK(!parse({"new", "demo", "--engine"}, o, e) && e.find("--engine") != std::string::npos);
    CHECK(!parse({"new", "demo", "--unique-id"}, o, e) && e.find("--unique-id") != std::string::npos);
    CHECK(!parse({"new", "demo", "--online"}, o, e) && e.find("--online") != std::string::npos);
    CHECK(!parse({"build", "demo", "--title", "x"}, o, e) && e.find("--title") != std::string::npos);
}

NNH_TEST(cli_parses_doctor_and_its_options) {
    Options o;
    std::string e;
    CHECK(parse({"doctor"}, o, e) && o.command == Command::Doctor && !o.online && !o.pc && o.console.empty());
    CHECK(parse({"doctor", "--online", "--pc", "--console", "192.168.10.52"}, o, e));
    CHECK(o.online && o.pc && o.console == "192.168.10.52");
    const std::string help = help_text();
    CHECK(help.find("nnh doctor [--online] [--console <ip>] [--pc]") != std::string::npos);
}

NNH_TEST(cli_refuses_mistakes_in_doctor) {
    Options o;
    std::string e;
    CHECK(!parse({"doctor", "hello"}, o, e) && e.find("doctor") != std::string::npos); // no game
    CHECK(!parse({"doctor", "--release"}, o, e) && e.find("--release") != std::string::npos);
    CHECK(!parse({"doctor", "--console"}, o, e) && e.find("--console") != std::string::npos);
    CHECK(!parse({"doctor", "--console", "8.8.8.8"}, o, e) && e.find("local network") != std::string::npos);
    CHECK(!parse({"doctor", "--console", "console.local"}, o, e) && e.find("IPv4") != std::string::npos);
    CHECK(!parse({"doctor", "--title", "x"}, o, e) && e.find("--title") != std::string::npos);
    CHECK(!parse({"run", "hello", "--online"}, o, e) && e.find("--online") != std::string::npos);
}
