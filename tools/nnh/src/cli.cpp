#include "cli.hpp"

namespace nnh::tool {

bool parse_command_line(const std::vector<std::string>& args, Options& out, std::string& error) {
    out = Options{};
    error.clear();
    if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") return true;
    const std::string& command = args[0];
    if (command == "build") {
        out.command = Command::Build;
    } else if (command == "run") {
        out.command = Command::Run;
    } else if (command == "install") {
        out.command = Command::Install;
    } else if (command == "serve" || command == "unpair") {
        out.command = command == "serve" ? Command::Serve : Command::Unpair;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (command == "serve" && args[i] == "--release") {
                out.release = true;
            } else {
                error = "nnh " + command + " takes no '" + args[i] + "'" +
                        (command == "serve" ? " (the console picks the game; --release is the only option)" : "");
                return false;
            }
        }
        return true;
    } else {
        error = "unknown command '" + command + "' (nnh help lists them)";
        return false;
    }
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--release") {
            out.release = true;
        } else if (a == "--pc") {
            out.pc = true;
        } else if (a == "--console") {
            if (i + 1 >= args.size()) {
                error = "--console needs the console's IP address";
                return false;
            }
            out.console = args[++i];
        } else if (!a.empty() && a[0] == '-') {
            error = "unknown option '" + a + "'";
            return false;
        } else if (out.game.empty()) {
            out.game = a;
        } else {
            error = "one game at a time ('" + out.game + "' and '" + a + "')";
            return false;
        }
    }
    if (out.game.empty()) {
        error = "which game? e.g. nnh " + command + " hello";
        return false;
    }
    if (out.command == Command::Install && out.pc) {
        error = "install puts a .cia on the console: --pc doesn't apply";
        return false;
    }
    return true;
}

const char* help_text() {
    return "nnh — the NNHGE tool (specs 010, 011)\n"
           "\n"
           "  nnh build <game> [--release] [--pc]   build a game for the 3DS (Development) or the PC\n"
           "  nnh run <game> [--release] [--console <ip>] [--pc]\n"
           "                                        build, send to the Homebrew Launcher (press Y on the\n"
           "                                        console) and show the game's log; --pc runs the simulator\n"
           "  nnh install <game> [--console <ip>]   build the .cia and install it with FBI (Remote Install ->\n"
           "                                        Receive URLs over the network)\n"
           "  nnh serve [--release]                 wait for NNH Link on the console: it lists the games,\n"
           "                                        asks for builds and starts them (spec 011); Ctrl+C stops\n"
           "  nnh unpair                            forget every console paired with nnh serve\n"
           "  nnh help                              this text\n"
           "\n"
           "<game> is the target name given to nnh_add_game(). The console is found on the local network\n"
           "and remembered; --console sets it. Logs are saved in build/logs/.\n";
}

} // namespace nnh::tool
