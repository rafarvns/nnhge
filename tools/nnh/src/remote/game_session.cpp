// The session loop (see game_session.hpp). One thread: it polls the socket in short waits, so typed lines,
// the Hello timeout and the linger after a verdict are handled between reads.

#include "remote/game_session.hpp"

#include <chrono>
#include <filesystem>
#include <vector>

#include "log_view.hpp"
#include "remote/game_stream.hpp"
#include "remote/png_writer.hpp"
#include "remote/script_runner.hpp"
#include "remote/stdin_lines.hpp"

namespace nnh::tool {
namespace {

using Clock = std::chrono::steady_clock;

/// How long one wait for the game's bytes lasts: typed lines and timeouts are looked at in between.
constexpr int kPollMs = 20;

long long ms_since(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

class Session {
public:
    Session(net::Socket& game, const SessionConfig& config) : game_(game), c_(config) {}

    int run() {
        if (c_.runner != nullptr) {
            c_.runner->start();
            flush();
        }
        while (!closed_ && !script_done()) {
            take_typed();
            check_hello();
            flush();
            if (!script_done()) receive(kPollMs);
        }
        const Clock::time_point verdict = Clock::now();
        while (!closed_ && ms_since(verdict) < c_.linger_ms) receive(kPollMs);
        return c_.runner != nullptr ? c_.runner->exit_code() : 0;
    }

private:
    net::Socket& game_;
    const SessionConfig& c_;
    remote::GameStream stream_;
    bool closed_ = false;
    bool hello_waiting_ = false;
    bool hello_reported_ = false;
    Clock::time_point hello_since_;

    bool script_done() const {
        return c_.runner != nullptr && !c_.runner->interactive() && c_.runner->finished();
    }

    void write(const std::string& terminal, const std::string& saved) {
        std::fprintf(c_.out, "%s\n", terminal.c_str());
        std::fflush(c_.out);
        if (c_.log != nullptr) {
            std::fprintf(c_.log, "%s\n", saved.c_str());
            std::fflush(c_.log);
        }
    }

    void note(const std::string& text) { write("nnh: " + text, "nnh: " + text); }

    void take_typed() {
        if (c_.typed == nullptr || c_.runner == nullptr) return;
        std::string line;
        while (c_.typed->take(line)) c_.runner->on_typed(line);
    }

    /// The game has hello_timeout_ms to answer each Hello; no answer is reported once.
    void check_hello() {
        if (c_.runner == nullptr || !c_.runner->hello_sent() || c_.runner->attached()) {
            hello_waiting_ = false;
            hello_reported_ = false;
            return;
        }
        if (!hello_waiting_) {
            hello_waiting_ = true;
            hello_since_ = Clock::now();
        } else if (!hello_reported_ && ms_since(hello_since_) >= c_.hello_timeout_ms) {
            hello_reported_ = true;
            c_.runner->no_answer();
        }
    }

    void receive(int wait_ms) {
        if (!net::readable(game_, wait_ms)) return;
        std::uint8_t buffer[64 * 1024];
        const long n = game_.recv_some(buffer, sizeof buffer);
        std::vector<remote::StreamItem> items;
        if (n <= 0) {
            closed_ = true;
            stream_.finish(items);
            deliver(items);
            if (c_.runner != nullptr) c_.runner->on_closed();
            flush();
            return;
        }
        stream_.feed(buffer, static_cast<std::size_t>(n), items);
        deliver(items);
    }

    void deliver(std::vector<remote::StreamItem>& items) {
        for (remote::StreamItem& item : items) {
            if (item.is_record) {
                if (c_.runner != nullptr) c_.runner->on_record(std::move(item.record));
            } else {
                write(render_log_line(item.line, c_.colour), item.line);
                if (c_.runner != nullptr) c_.runner->on_line(item.line);
            }
            flush();
        }
    }

    /// What the runner handed out: messages go to the game (one write), screenshots to files, notes out.
    void flush() {
        if (c_.runner == nullptr) return;
        std::vector<std::uint8_t> bytes;
        for (const remote::MessageBytes& m : c_.runner->take_messages()) bytes.insert(bytes.end(), m.begin(), m.end());
        if (!bytes.empty() && !closed_) game_.send_all(bytes.data(), bytes.size()); // a failure shows as a close
        for (const remote::ScreenJob& job : c_.runner->take_screens()) save(job);
        for (const std::string& text : c_.runner->take_notes()) note(text);
    }

    void save(const remote::ScreenJob& job) {
        std::error_code ec;
        std::filesystem::create_directories(c_.shots_dir, ec);
        const std::string path = c_.shots_dir + "/" + job.file_name;
        const std::vector<std::uint8_t> png = remote::encode_png(job.image);
        std::FILE* f = std::fopen(path.c_str(), "wb");
        const bool ok = f != nullptr && std::fwrite(png.data(), 1, png.size(), f) == png.size();
        if (f != nullptr && std::fclose(f) != 0) f = nullptr;
        const std::string what = "line " + std::to_string(job.line) + " screenshot " + job.name;
        note(ok && f != nullptr ? what + " -> " + path : what + ": can't write " + path);
    }
};

} // namespace

int run_game_session(net::Socket& game, const SessionConfig& config) { return Session(game, config).run(); }

} // namespace nnh::tool
