#include "storage.hpp"

#include <cstring>

#include "nnh_link/messages.hpp"

namespace link_app {
namespace {

// Format 1: console id (16) | pair count (1) | count x (server id 16, secret 32) |
//           has last server (1) [server id 16, address 4] | last game length (1) | last game.
class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& b) : b_(b) {}
    bool take(void* out, std::size_t n) {
        if (n > b_.size() - at_) return false;
        std::memcpy(out, b_.data() + at_, n);
        at_ += n;
        return true;
    }
    bool byte(std::uint8_t& v) { return take(&v, 1); }
    bool done() const { return at_ == b_.size(); }

private:
    const std::vector<std::uint8_t>& b_;
    std::size_t at_ = 0;
};

void put(std::vector<std::uint8_t>& out, const void* data, std::size_t n) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    out.insert(out.end(), p, p + n);
}

} // namespace

std::optional<Digest> Storage::secret_of(const Id& server) {
    for (const auto& [id, secret] : pairs_) {
        if (id == server) return secret;
    }
    return std::nullopt;
}

void Storage::remember(const Id& server, const Digest& secret) {
    forget(server);
    if (pairs_.size() == kMaxPairs) pairs_.erase(pairs_.begin());
    pairs_.emplace_back(server, secret);
    dirty_ = true;
}

bool Storage::paired(const Id& server) const {
    for (const auto& p : pairs_) {
        if (p.first == server) return true;
    }
    return false;
}

void Storage::forget(const Id& server) {
    for (auto it = pairs_.begin(); it != pairs_.end(); ++it) {
        if (it->first == server) {
            pairs_.erase(it);
            dirty_ = true;
            return;
        }
    }
}

bool Storage::has_console_id() const { return console_id_ != Id{}; }

void Storage::set_console_id(const Id& id) {
    console_id_ = id;
    dirty_ = true;
}

void Storage::set_last_server(const Id& id, Ipv4 address) {
    if (last_server_ && last_server_->id == id && last_server_->address == address) return;
    last_server_ = LastServer{id, address};
    dirty_ = true;
}

void Storage::set_last_game(const std::string& game) {
    if (game == last_game_ || !nnh::link::valid_game_name(game)) return;
    last_game_ = game;
    dirty_ = true;
}

std::vector<std::uint8_t> Storage::encode() const {
    std::vector<std::uint8_t> out;
    put(out, console_id_.data(), console_id_.size());
    out.push_back(static_cast<std::uint8_t>(pairs_.size()));
    for (const auto& [id, secret] : pairs_) {
        put(out, id.data(), id.size());
        put(out, secret.data(), secret.size());
    }
    out.push_back(last_server_ ? 1 : 0);
    if (last_server_) {
        put(out, last_server_->id.data(), last_server_->id.size());
        put(out, last_server_->address.b, 4);
    }
    out.push_back(static_cast<std::uint8_t>(last_game_.size()));
    put(out, last_game_.data(), last_game_.size());
    return out;
}

bool Storage::decode(const std::vector<std::uint8_t>& bytes) {
    Reader r(bytes);
    Storage s;
    std::uint8_t count = 0;
    if (!r.take(s.console_id_.data(), s.console_id_.size()) || !r.byte(count) || count > kMaxPairs) return false;
    for (std::uint8_t i = 0; i < count; ++i) {
        Id id;
        Digest secret;
        if (!r.take(id.data(), id.size()) || !r.take(secret.data(), secret.size()) || s.paired(id)) return false;
        s.pairs_.emplace_back(id, secret);
    }
    std::uint8_t has_last = 0;
    if (!r.byte(has_last) || has_last > 1) return false;
    if (has_last == 1) {
        LastServer last;
        if (!r.take(last.id.data(), last.id.size()) || !r.take(last.address.b, 4)) return false;
        s.last_server_ = last;
    }
    std::uint8_t length = 0;
    if (!r.byte(length)) return false;
    s.last_game_.resize(length);
    if (!r.take(s.last_game_.data(), length) || !r.done()) return false;
    if (length > 0 && !nnh::link::valid_game_name(s.last_game_)) return false;
    *this = static_cast<Storage&&>(s);
    return true;
}

} // namespace link_app
