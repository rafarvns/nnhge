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
