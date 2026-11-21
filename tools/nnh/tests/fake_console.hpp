#pragma once
// A fake console for the script runner's tests (spec 013 T3.5): it plays the console side of the remote
// protocol frame by frame — Hello answer and ticks every 4th frame (as remote_session.cpp), due messages
// applied at the start of their frame with Late for past ones, screenshots at the end of the frame — and a
// tiny game that logs "pressed <mask>" when a remote press is applied. Records are built with core's
// encoders and go through the PC's own stream splitter, as in a real run.

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "remote/game_stream.hpp"
#include "remote/script_runner.hpp"

namespace nnh::tool::test {

namespace rr = nnh::tool::remote;
namespace wire = nnh::detail::remote;

struct Applied {
    std::uint32_t frame = 0; ///< the frame it was applied on
    wire::Message message;
};

struct FakeConsole {
    explicit FakeConsole(rr::ScriptRunner& r) : runner(r) {}

    rr::ScriptRunner& runner;
    std::uint32_t frame = 0;            ///< the frame that ran last
    std::uint32_t capabilities = 0x3F;  ///< everything (a New 3DS)
    bool attached = false;
    bool quit = false;
    int byes = 0;
    std::size_t most_waiting = 0;       ///< the largest number of messages queued at once
    std::map<std::uint32_t, std::size_t> waiting_at; ///< messages queued at the start of each frame
    std::vector<wire::Message> received;
    std::vector<wire::Message> waiting; ///< queued, not yet applied
    std::vector<Applied> applied;
    std::map<std::uint32_t, std::vector<std::string>> logs; ///< extra lines the game logs on a frame
    std::vector<std::string> notes;
    std::vector<rr::ScreenJob> screens;

    /// Runs `n` frames.
    void run(int n) {
        for (int i = 0; i < n; ++i) next_frame();
    }

    /// Runs frames until the runner finishes (at most `limit`).
    void run_until_finished(int limit = 100000) {
        for (int i = 0; i < limit && !runner.finished(); ++i) next_frame();
    }

    /// A line of this frame, in the engine's format.
    void log(const std::string& text, char level = 'I') {
        char line[300];
        std::snprintf(line, sizeof line, "[f%-8u 0.000s] %c game   %s", frame, level, text.c_str());
        to_pc_line(line);
    }

    /// Sends a record (bytes from core's encoders) through the PC's stream splitter.
    void to_pc(const std::uint8_t* bytes, std::size_t size) {
        std::vector<rr::StreamItem> items;
        stream.feed(bytes, size, items);
        deliver(items);
    }

    void to_pc_line(const std::string& line) {
        std::string bytes = line + "\n";
        to_pc(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    }

    void collect() {
        for (std::string& n : runner.take_notes()) notes.push_back(n);
        for (rr::ScreenJob& j : runner.take_screens()) screens.push_back(std::move(j));
    }

    bool noted(const std::string& part) const {
        for (const std::string& n : notes) {
            if (n.find(part) != std::string::npos) return true;
        }
        return false;
    }

    /// Messages of one type received so far.
    std::vector<wire::Message> of_type(wire::MessageType type) const {
        std::vector<wire::Message> out;
        for (const wire::Message& m : received) {
            if (m.type == type) out.push_back(m);
        }
        return out;
    }

private:
    rr::GameStream stream;
    std::uint32_t next_tick = 0;
    std::vector<wire::Message> shots; ///< screenshots due at the end of this frame

    void deliver(std::vector<rr::StreamItem>& items) {
        for (rr::StreamItem& item : items) {
            if (item.is_record) runner.on_record(std::move(item.record));
            else runner.on_line(item.line);
        }
        collect();
    }

    void record(std::size_t size, const std::uint8_t* bytes) { to_pc(bytes, size); }

    void next_frame() {
        ++frame;
        std::uint8_t out[wire::kMaxFixedRecord];
        for (const rr::MessageBytes& bytes : runner.take_messages()) {
            wire::Message m;
            if (wire::decode_message(bytes.data(), m) != wire::Reject::None) continue;
            received.push_back(m);
            if (m.type == wire::MessageType::Hello) {
                attached = true;
                record(wire::encode_hello_record(frame, capabilities, "Development", out), out);
                record(wire::encode_tick_record(frame, out), out);
                next_tick = frame + 4;
            } else if (attached) {
                waiting.push_back(m);
            }
        }
        if (waiting.size() > most_waiting) most_waiting = waiting.size();
        waiting_at[frame] = waiting.size();
        apply_due(out);
        if (attached && frame >= next_tick) {
            record(wire::encode_tick_record(frame, out), out);
            next_tick = frame + 4;
        }
        for (const std::string& text : logs[frame]) log(text);
        for (const wire::Message& m : shots) send_screens(m);
        shots.clear();
        collect();
    }

    void apply_due(std::uint8_t* out) {
        while (!waiting.empty() && wire::resolve_frame(waiting.front().frame, frame) <= frame) {
            const wire::Message m = waiting.front();
            waiting.erase(waiting.begin());
            if (m.frame != 0 && m.frame < frame) record(wire::encode_late_record(m.seq, m.frame, frame, out), out);
            applied.push_back({frame, m});
            if (m.type == wire::MessageType::Buttons && m.press != 0) log("pressed " + std::to_string(m.press));
            if (m.type == wire::MessageType::Screenshot) shots.push_back(m);
            if (m.type == wire::MessageType::Bye) {
                ++byes;
                quit = quit || m.quit;
                attached = false;
                waiting.clear();
                return;
            }
        }
    }

    void send_screens(const wire::Message& m) {
        for (std::uint8_t screen = 0; screen < 2; ++screen) {
            if ((m.screens & (1u << screen)) == 0) continue;
            wire::ScreenHeader h{.id = m.id, .screen = screen, .layout = 1, .frame = frame,
                                 .width = static_cast<std::uint16_t>(screen == 0 ? 400 : 320), .height = 240};
            const std::uint32_t bytes = h.width * h.height * 4u;
            std::uint8_t head[wire::kMaxFixedRecord];
            std::vector<std::uint8_t> all(head, head + wire::encode_screen_record(h, bytes, head));
            all.resize(all.size() + bytes, 0x7F);
            to_pc(all.data(), all.size());
        }
    }
};

} // namespace nnh::tool::test
