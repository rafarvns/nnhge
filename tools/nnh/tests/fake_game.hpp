#pragma once
// A fake Development game for the session loop's tests (spec 013 T4.3): it connects to nnh over the loopback
// interface and plays the console side for real bytes — reads 16-byte messages without waiting, answers
// Hello, ticks every 4th frame, applies due messages (logging "pressed <mask>"), sends screenshots (layout 1)
// at the end of their frame and, on a Bye with quit, logs "bye" and closes, like the simulator does.

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "link_types.hpp"
#include "remote/remote_wire.hpp"

namespace nnh::tool::test {

namespace wire = nnh::detail::remote;

struct FakeGame {
    bool answer_hello = true;
    int frames = 400;          ///< frames it runs before closing on its own
    int close_after_hello = 0; ///< > 0: closes this many frames after answering Hello
    std::uint32_t late_line_frame = 0; ///< > 0: logs "late line" on this frame
    std::uint32_t tick_every = 4;      ///< the console's kTickEvery
    std::vector<std::string> first_lines; ///< written right after connecting
    int init_lines = 0;   ///< a slow init before frame 1: this many "loading" lines...
    int init_gap_ms = 0;  ///< ...each after this long, reading nothing meanwhile
    std::vector<wire::Message> received;

    void run(std::uint16_t port) {
        socket = net::tcp_connect(Ipv4{{127, 0, 0, 1}}, port, 2000);
        for (const std::string& line : first_lines) socket.send_all(line.data(), line.size());
        for (int i = 0; i < init_lines; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(init_gap_ms));
            log("loading " + std::to_string(i));
        }
        for (frame = 1; frame <= static_cast<std::uint32_t>(frames) && socket.valid(); ++frame) {
            read_messages();
            if (!socket.valid()) break;
            // As the console: due messages and the tick at the start of the frame, screenshots at its end.
            std::vector<wire::Message> shots = apply_due();
            if (attached && frame >= next_tick) tick();
            if (frame == late_line_frame) log("late line");
            for (const wire::Message& m : shots) send_screens(m);
            if (attached_at != 0 && close_after_hello > 0 && frame >= attached_at + close_after_hello) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        socket.close();
    }

private:
    net::Socket socket;
    std::uint32_t frame = 0;
    std::uint32_t next_tick = 0;
    std::uint32_t attached_at = 0;
    bool attached = false;
    std::vector<std::uint8_t> pending;
    std::vector<wire::Message> waiting;

    void send(const std::uint8_t* bytes, std::size_t size) { socket.send_all(bytes, size); }

    void log(const std::string& text) {
        char line[200];
        const int n = std::snprintf(line, sizeof line, "[f%-8u 0.000s] I game   %s\n", frame, text.c_str());
        send(reinterpret_cast<const std::uint8_t*>(line), static_cast<std::size_t>(n));
    }

    void tick() {
        std::uint8_t out[wire::kMaxFixedRecord];
        send(out, wire::encode_tick_record(frame, out));
        next_tick = frame + tick_every;
    }

    void read_messages() {
        std::uint8_t buffer[512];
        while (net::readable(socket, 0)) {
            const long n = socket.recv_some(buffer, sizeof buffer);
            if (n <= 0) {
                socket.close();
                return;
            }
            pending.insert(pending.end(), buffer, buffer + n);
        }
        while (pending.size() >= wire::kMessageSize) {
            wire::Message m;
            if (wire::decode_message(pending.data(), m) == wire::Reject::None) handle(m);
            pending.erase(pending.begin(), pending.begin() + wire::kMessageSize);
        }
    }

    void handle(const wire::Message& m) {
        received.push_back(m);
        if (m.type != wire::MessageType::Hello) {
            if (attached) waiting.push_back(m);
            return;
        }
        if (!answer_hello) return;
        attached = true;
        attached_at = frame;
        std::uint8_t out[wire::kMaxFixedRecord];
        send(out, wire::encode_hello_record(frame, 0x3F, "Development", out));
        tick();
    }

    /// Applies the due messages. @return the screenshots to send at the end of this frame.
    std::vector<wire::Message> apply_due() {
        std::vector<wire::Message> shots;
        while (!waiting.empty() && wire::resolve_frame(waiting.front().frame, frame) <= frame) {
            const wire::Message m = waiting.front();
            waiting.erase(waiting.begin());
            if (m.type == wire::MessageType::Buttons && m.press != 0) log("pressed " + std::to_string(m.press));
            if (m.type == wire::MessageType::Screenshot) shots.push_back(m);
            if (m.type == wire::MessageType::Bye) {
                attached = false;
                waiting.clear();
                if (m.quit) {
                    log("bye");
                    socket.close();
                }
                return {};
            }
        }
        return shots;
    }

    void send_screens(const wire::Message& m) {
        for (std::uint8_t screen = 0; screen < 2; ++screen) {
            if ((m.screens & (1u << screen)) == 0) continue;
            wire::ScreenHeader h{.id = m.id, .screen = screen, .layout = 1, .frame = frame,
                                 .width = static_cast<std::uint16_t>(screen == 0 ? 400 : 320), .height = 240};
            const std::uint32_t bytes = h.width * h.height * 4u;
            std::uint8_t head[wire::kMaxFixedRecord];
            std::vector<std::uint8_t> all(head, head + wire::encode_screen_record(h, bytes, head));
            all.resize(all.size() + bytes, 0x40);
            send(all.data(), all.size());
        }
    }
};

} // namespace nnh::tool::test
