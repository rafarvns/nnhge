// Mini-fuzz of the tool's untrusted inputs (spec 013 T3.7, FR-8): the game's stream (bytes from the network),
// scripts (files), a game project's nnh.toml and presets files (spec 032, constitution X). Seeded, deterministic
// mutations; each run checks invariants, not exact output.

#include <cstdint>
#include <string>
#include <vector>

#include "preset_names.hpp"
#include "project_file.hpp"
#include "remote/game_stream.hpp"
#include "remote/script.hpp"
#include "remote/script_timeline.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
namespace wire = nnh::detail::remote;

namespace {

struct Rng {
    std::uint32_t state;
    std::uint32_t next() { return state = state * 1664525u + 1013904223u; }
};

template <typename Bytes>
void mutate(Bytes& s, Rng& rng) {
    const int edits = 1 + static_cast<int>(rng.next() % 6);
    for (int e = 0; e < edits; ++e) {
        const std::uint32_t r = rng.next();
        const std::size_t at = s.empty() ? 0 : (r >> 8) % s.size();
        const auto byte = static_cast<typename Bytes::value_type>(rng.next() >> 16);
        switch (r % 4) {
        case 0: if (!s.empty()) s[at] = byte; break;
        case 1: if (!s.empty()) s.erase(s.begin() + static_cast<long>(at)); break;
        case 2: s.insert(s.begin() + static_cast<long>(at), byte); break;
        default: if (!s.empty()) s[at] = static_cast<typename Bytes::value_type>(at % 3 == 0 ? 0x1E : '\n'); break;
        }
    }
}

std::vector<std::uint8_t> stream_seed() {
    std::vector<std::uint8_t> out;
    const std::string text = "[f1 0.0s] I app hello\r\npartial";
    out.insert(out.end(), text.begin(), text.end());
    std::uint8_t r[wire::kMaxFixedRecord];
    out.insert(out.end(), r, r + wire::encode_hello_record(1, 0x3F, "Development", r));
    out.insert(out.end(), r, r + wire::encode_tick_record(5, r));
    const wire::ScreenHeader h{.id = 1, .screen = 0, .layout = 1, .frame = 5, .width = 2, .height = 2};
    out.insert(out.end(), r, r + wire::encode_screen_record(h, 16, r));
    out.insert(out.end(), 16, 0x1E);
    out.insert(out.end(), r, r + wire::encode_late_record(3, 4, 5, r));
    out.push_back('\n');
    return out;
}

std::vector<StreamItem> split(const std::vector<std::uint8_t>& bytes, Rng* chunks) {
    GameStream stream;
    std::vector<StreamItem> items;
    for (std::size_t at = 0; at < bytes.size();) {
        std::size_t n = chunks ? 1 + chunks->next() % 13 : bytes.size();
        if (n > bytes.size() - at) n = bytes.size() - at;
        stream.feed(bytes.data() + at, n, items);
        at += n;
    }
    stream.finish(items);
    return items;
}

bool same(const std::vector<StreamItem>& a, const std::vector<StreamItem>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].is_record != b[i].is_record || a[i].line != b[i].line) return false;
        if (a[i].is_record && (a[i].record.type != b[i].record.type || a[i].record.pixels != b[i].record.pixels ||
                               a[i].record.frame != b[i].record.frame)) {
            return false;
        }
    }
    return true;
}

/// Every value of a parsed step is within the script's limits.
bool step_ok(const Step& s) {
    const bool frames_ok = s.frames >= 1 && s.frames <= kMaxFrames;
    switch (s.command) {
    case Command::Wait:
    case Command::Expect: return frames_ok;
    case Command::Hold: return frames_ok && s.buttons != 0 && (s.buttons & ~wire::kButtonMask) == 0;
    case Command::Press:
    case Command::Release:
    case Command::Tap: return s.buttons != 0 && (s.buttons & ~wire::kButtonMask) == 0;
    case Command::Stick: return s.stick <= 1 && s.x >= -1000 && s.x <= 1000 && s.y >= -1000 && s.y <= 1000;
    case Command::Touch: return s.x >= 0 && s.x <= 319 && s.y >= 0 && s.y <= 239;
    case Command::Drag: return frames_ok && s.x >= 0 && s.x <= 319 && s.y >= 0 && s.y <= 239;
    case Command::Screenshot: return !s.text.empty() && s.text.size() <= kMaxNameChars;
    case Command::FailOn: return !s.text.empty() && s.text.size() <= kMaxTextChars;
    default: return true;
    }
}

const char* const kScriptSeed = "# smoke\nfail-on \"] E \"\nwait 30\ntap A\nexpect \"pressed A\" 10\nwait 10\n"
                                "hold L+R 20\nstick circle 1 -0.5\nstick cstick 0 0\ntouch 40 40\ndrag 280 200 30\n"
                                "screenshot dragged\nuntouch\nrelease all\nexpect \"touch \\\"ended\\\"\" 5\nend\n";

} // namespace

NNH_TEST(stream_target_survives_mutated_inputs) {
    const std::vector<std::uint8_t> seed = stream_seed();
    Rng rng{2026};
    for (int i = 0; i < 4000; ++i) {
        std::vector<std::uint8_t> bytes = seed;
        mutate(bytes, rng);
        const std::vector<StreamItem> whole = split(bytes, nullptr);
        CHECK(same(whole, split(bytes, &rng))); // chunking never changes the result
        std::size_t out = 0;
        for (const StreamItem& item : whole) {
            if (item.is_record) {
                out += wire::kRecordHeaderSize;
                if (item.record.type == wire::RecordType::Screen) out += wire::kScreenFixed + item.record.pixels.size();
            } else {
                CHECK(item.line.find('\n') == std::string::npos && item.line.find('\r') == std::string::npos);
                out += item.line.size();
            }
        }
        CHECK(out <= bytes.size()); // nothing is invented
    }
}

NNH_TEST(script_target_survives_mutated_inputs) {
    const std::string seed = kScriptSeed;
    CHECK(parse_script(seed, "f.nnhs").errors.empty());
    Rng rng{13};
    for (int i = 0; i < 6000; ++i) {
        std::string text = seed;
        mutate(text, rng);
        const ParsedScript p = parse_script(text, "f.nnhs");
        for (const std::string& e : p.errors) CHECK(e.rfind("f.nnhs:", 0) == 0);
        if (!p.errors.empty()) continue;
        CHECK(!p.script.steps.empty() && p.script.steps.back().command == Command::End);
        for (const Step& s : p.script.steps) CHECK(step_ok(s) && s.line >= 1 && s.line <= p.script.lines + 1);
        for (const Segment& seg : build_timeline(p.script)) {
            std::uint32_t last = 0;
            for (const TimedEvent& e : seg.events) {
                CHECK(e.offset >= last && e.offset <= seg.length); // the console refuses frames going back
                last = e.offset;
            }
        }
    }
}

NNH_TEST(project_file_target_survives_mutated_inputs) {
    const std::string seed = "# a game project\nname = \"mygame\"\nengine = \"1.0.0\"\n[later]\nkey = 3\n";
    CHECK(nnh::tool::read_project_file(seed).errors.empty());
    Rng rng{32};
    for (int i = 0; i < 6000; ++i) {
        std::string text = seed;
        mutate(text, rng);
        const nnh::tool::ProjectFile f = nnh::tool::read_project_file(text);
        for (const std::string& e : f.errors) CHECK(e.rfind("nnh.toml:", 0) == 0);
        for (const std::string& w : f.warnings) CHECK(w.rfind("nnh.toml:", 0) == 0);
        CHECK(f.name.size() <= text.size() && f.engine.size() <= text.size()); // nothing is invented
    }
}

NNH_TEST(presets_target_survives_mutated_inputs) {
    const std::string seed = R"({ "version": 6, "configurePresets": [ { "name": "pc-dev", "inherits": ["base"] } ],
                                  "buildPresets": [ { "name": "pc-dev", "configurePreset": "pc-dev" } ] })";
    CHECK(nnh::tool::presets_define(seed, "", "pc-dev"));
    Rng rng{55};
    int found = 0;
    for (int i = 0; i < 6000; ++i) {
        std::string text = seed;
        mutate(text, rng);
        found += nnh::tool::presets_define(text, "", "pc-dev") ? 1 : 0; // never crashes or loops
    }
    CHECK(found > 0 && found < 6000);
}
