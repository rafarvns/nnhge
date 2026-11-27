// The options of nnh new and nnh doctor (cli.hpp, spec 033 FR-2). Values are taken as typed: nnh new checks
// the name, title, author, engine folder and unique ID itself (exit 1); only the command line's shape is
// checked here (exit 2).

#include "cli.hpp"
#include "link_types.hpp"

namespace nnh::tool::cli_parts {
namespace {

/// The value after option `args[i]` into `out`; false with `error` set when there is none.
bool value_of(const std::vector<std::string>& args, std::size_t& i, std::string& out, std::string& error,
              const char* what) {
    if (i + 1 >= args.size()) {
        error = args[i] + " needs " + what;
        return false;
    }
    out = args[++i];
    return true;
}

} // namespace

bool parse_new(const std::vector<std::string>& args, Options& out, std::string& error) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        bool ok = true;
        if (a == "--title") {
            ok = value_of(args, i, out.title, error, "a title, e.g. --title \"My Game\"");
            out.has_title = true;
        } else if (a == "--author") {
            ok = value_of(args, i, out.author, error, "a name, e.g. --author \"Me\"");
        } else if (a == "--engine") {
            ok = value_of(args, i, out.engine, error, "an engine folder (an NNHGE checkout)");
        } else if (a == "--unique-id") {
            ok = value_of(args, i, out.unique_id, error, "a number from 0x00300 to 0xF7FFF");
        } else if (!a.empty() && a[0] == '-') {
            error = "unknown option '" + a + "' for nnh new (--title, --author, --engine, --unique-id)";
            return false;
        } else if (out.game.empty()) {
            out.game = a;
        } else {
            error = "one name at a time ('" + out.game + "' and '" + a + "')";
            return false;
        }
        if (!ok) return false;
    }
    if (out.game.empty()) {
        error = "which name? e.g. nnh new mygame";
        return false;
    }
    return true;
}

bool parse_doctor(const std::vector<std::string>& args, Options& out, std::string& error) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--online") {
            out.online = true;
        } else if (a == "--pc") {
            out.pc = true;
        } else if (a == "--console") {
            if (!value_of(args, i, out.console, error, "the console's IP address")) return false;
            const auto address = parse_ipv4(out.console);
            if (!address) {
                error = "--console " + out.console + " is not an IPv4 address (e.g. 192.168.1.20)";
                return false;
            }
            if (!is_local_network(*address)) {
                error =
                    "--console " + out.console + " is not on the local network (nnh only talks to the LAN)";
                return false;
            }
        } else if (!a.empty() && a[0] == '-') {
            error = "unknown option '" + a + "' for nnh doctor (--online, --console <ip>, --pc)";
            return false;
        } else {
            error = "nnh doctor takes no '" + a + "' (it checks this computer and the project you are in)";
            return false;
        }
    }
    return true;
}

} // namespace nnh::tool::cli_parts
