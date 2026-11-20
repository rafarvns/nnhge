#include "serve/serve_hooks.hpp"

#include <cstdio>

#include "commands.hpp"
#include "repo.hpp"

namespace nnh::tool {

std::string short_id(const Id& id) { return to_hex(id.data(), 4); }

ServeHooks::ServeHooks(std::string root, bool release, std::string pairs_path, Clock clock, Random random)
    : root_(static_cast<std::string&&>(root)),
      release_(release),
      pairs_path_(static_cast<std::string&&>(pairs_path)),
      clock_(static_cast<Clock&&>(clock)),
      random_(static_cast<Random&&>(random)),
      pairs_(load_pairs(pairs_path_)) {}

std::optional<Digest> ServeHooks::secret_of(const Id& console) {
    for (const auto& [id, secret] : pairs_) {
        if (id == console) {
            std::printf("nnh: console %s is connecting\n", short_id(console).c_str());
            return secret;
        }
    }
    const std::int64_t now = clock_();
    std::string code = codes_.current(now);
    if (code.empty()) code = codes_.renew(now, random_());
    std::printf("nnh: a new console (%s) wants to pair: enter %s on it (valid for 2 minutes)\n",
                short_id(console).c_str(), code.c_str());
    return std::nullopt;
}

void ServeHooks::remember(const Id& console, const Digest& secret) {
    codes_.spend(); // one pairing per code
    bool replaced = false;
    for (auto& p : pairs_) {
        if (p.first == console) {
            p.second = secret;
            replaced = true;
        }
    }
    if (!replaced) pairs_.emplace_back(console, secret);
    if (save_pairs(pairs_path_, pairs_)) {
        std::printf("nnh: console %s paired\n", short_id(console).c_str());
    } else {
        std::fprintf(stderr, "nnh: console %s paired, but %s can't be written: it must pair again next time\n",
                     short_id(console).c_str(), pairs_path_.c_str());
    }
}

std::string ServeHooks::pairing_code() {
    const std::int64_t now = clock_();
    const std::string code = codes_.current(now);
    if (code.empty()) { // e.g. a console that lost its pairing: it typed a code before any was shown
        std::printf("nnh: a console wants to pair: enter %s on it (valid for 2 minutes)\n",
                    codes_.renew(now, random_()).c_str());
    }
    return code;
}

std::vector<link::GameInfo> ServeHooks::games() {
    std::vector<GameEntry> entries;
    std::vector<link::GameInfo> out;
    if (!configure_and_list(root_, preset_name(false, release_), true, entries)) return out;
    for (const GameEntry& g : entries) {
        if (link::valid_game_name(g.name)) out.push_back({g.name, g.title});
    }
    std::printf("nnh: the console asked for the games (%zu)\n", out.size());
    return out;
}

link::BuildOutput ServeHooks::build(const link::BuildRequest& request,
                                    const std::function<bool(const std::string&)>& line) {
    std::vector<std::string> last;
    const OutputFn output = [&](const std::string& text) {
        last.push_back(text);
        if (last.size() > link::kMaxErrorLines) last.erase(last.begin());
        return line(text);
    };
    Options options;
    options.game = request.game;
    const bool release = request.release || release_;
    BuiltGame built;
    link::BuildOutput out;
    out.ok = build_target(options, "_3dsx", release, built, false, &output);
    if (out.ok) {
        out.file = built.game.binary_dir + "/" + request.game + ".3dsx";
        std::printf("nnh: sending %s.3dsx to the console\n", request.game.c_str());
    } else {
        out.lines = last;
    }
    return out;
}

void ServeHooks::launching(const std::string& game) {
    std::printf("nnh: the console is starting %s\n", game.c_str());
    launch_ = game;
}

void ServeHooks::show_new_code() {
    std::printf("nnh: pairing code %s (valid for 2 minutes; a new one appears when a new console connects)\n",
                codes_.renew(clock_(), random_()).c_str());
}

std::string ServeHooks::take_launch() {
    std::string game = launch_;
    launch_.clear();
    return game;
}

} // namespace nnh::tool
