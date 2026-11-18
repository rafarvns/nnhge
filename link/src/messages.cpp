#include "nnh_link/messages.hpp"

#include "nnh_link/wire.hpp"

namespace nnh::link {

namespace {

/// Printable text only (UTF-8 bytes allowed), no control characters: what the console can show.
bool printable(const std::string& s) {
    for (unsigned char c : s) {
        if (c < 0x20 || c == 0x7F) return false;
    }
    return true;
}

} // namespace

bool valid_type(std::uint8_t t) { return t >= 1 && t <= static_cast<std::uint8_t>(Type::NeedPairing); }

bool valid_game_name(const std::string& name) {
    if (name.empty() || name.size() > kMaxName) return false;
    for (char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

std::vector<std::uint8_t> encode(const Hello& m) {
    Writer w;
    w.u16(m.version);
    w.raw(m.id.data(), m.id.size());
    w.raw(m.nonce.data(), m.nonce.size());
    return std::move(w.bytes());
}

bool decode(const std::vector<std::uint8_t>& p, Hello& m) {
    Reader r(p);
    m.version = r.u16();
    r.raw(m.id.data(), m.id.size());
    r.raw(m.nonce.data(), m.nonce.size());
    return r.done();
}

std::vector<std::uint8_t> encode_digest(const Digest& d) { return std::vector<std::uint8_t>(d.begin(), d.end()); }

bool decode_digest(const std::vector<std::uint8_t>& p, Digest& d) {
    Reader r(p);
    r.raw(d.data(), d.size());
    return r.done();
}

std::vector<std::uint8_t> encode_text(const std::string& s) {
    Writer w;
    w.str(s.size() > kMaxText ? s.substr(0, kMaxText) : s);
    return std::move(w.bytes());
}

bool decode_text(const std::vector<std::uint8_t>& p, std::string& s) {
    Reader r(p);
    s = r.str(kMaxText);
    return r.done() && printable(s);
}

std::vector<std::uint8_t> encode(const std::vector<GameInfo>& games) {
    Writer w;
    const std::size_t n = games.size() < kMaxGames ? games.size() : kMaxGames;
    w.u16(static_cast<std::uint16_t>(n));
    for (std::size_t i = 0; i < n; ++i) {
        w.str(games[i].name);
        w.str(games[i].title.size() > kMaxName ? games[i].title.substr(0, kMaxName) : games[i].title);
    }
    return std::move(w.bytes());
}

bool decode(const std::vector<std::uint8_t>& p, std::vector<GameInfo>& games) {
    Reader r(p);
    const std::size_t n = r.u16();
    if (n > kMaxGames) return false;
    games.clear();
    for (std::size_t i = 0; i < n && r.ok(); ++i) {
        GameInfo g;
        g.name = r.str(kMaxName);
        g.title = r.str(kMaxName);
        if (!valid_game_name(g.name) || !printable(g.title)) return false;
        games.push_back(std::move(g));
    }
    return r.done();
}

std::vector<std::uint8_t> encode(const BuildRequest& m) {
    Writer w;
    w.str(m.game);
    w.u8(m.release ? 1 : 0);
    return std::move(w.bytes());
}

bool decode(const std::vector<std::uint8_t>& p, BuildRequest& m) {
    Reader r(p);
    m.game = r.str(kMaxName);
    const std::uint8_t release = r.u8();
    m.release = release == 1;
    return r.done() && release <= 1 && valid_game_name(m.game);
}

std::vector<std::uint8_t> encode_lines(const std::vector<std::string>& lines) {
    Writer w;
    const std::size_t first = lines.size() > kMaxErrorLines ? lines.size() - kMaxErrorLines : 0; // the last ones
    w.u8(static_cast<std::uint8_t>(lines.size() - first));
    for (std::size_t i = first; i < lines.size(); ++i) w.str(lines[i].size() > kMaxText ? lines[i].substr(0, kMaxText) : lines[i]);
    return std::move(w.bytes());
}

bool decode_lines(const std::vector<std::uint8_t>& p, std::vector<std::string>& lines) {
    Reader r(p);
    const std::size_t n = r.u8();
    if (n > kMaxErrorLines) return false;
    lines.clear();
    for (std::size_t i = 0; i < n && r.ok(); ++i) {
        lines.push_back(r.str(kMaxText));
        if (!printable(lines.back())) return false;
    }
    return r.done();
}

std::vector<std::uint8_t> encode(const BuildResult& m) {
    Writer w;
    w.u64(m.size);
    w.raw(m.sha256.data(), m.sha256.size());
    return std::move(w.bytes());
}

bool decode(const std::vector<std::uint8_t>& p, BuildResult& m) {
    Reader r(p);
    m.size = r.u64();
    r.raw(m.sha256.data(), m.sha256.size());
    return r.done() && m.size <= 64ull * 1024 * 1024; // a .3dsx; the console's SD space is checked too
}

} // namespace nnh::link
