// Whole wire units for a reload's paced sending (see paced_units.hpp).

#include "reload/paced_units.hpp"

#include "reload/reload_wire.hpp"
#include "remote/remote_wire.hpp"

namespace nnh::tool::reload {

namespace wire = nnh::detail::remote;

void PacedUnits::clear() {
    bytes_.clear();
    ends_.clear();
    file_.clear();
    next_ = 0;
    tail_unit_ = static_cast<std::size_t>(-1);
}

void PacedUnits::append(const std::vector<std::uint8_t>& bytes) {
    const std::size_t base = bytes_.size();
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    std::size_t at = 0;
    while (at < bytes.size()) {
        std::size_t size = bytes.size() - at; // a malformed rest goes as one unit (never happens: the encoders' output)
        bool file = false;
        wire::Message m;
        if (bytes.size() - at >= wire::kMessageSize && wire::decode_message(bytes.data() + at, m) == wire::Reject::None) {
            size = wire::kMessageSize + wire::payload_length(m);
            if (size > bytes.size() - at) size = bytes.size() - at;
            file = m.type == wire::MessageType::ReloadFile;
        }
        at += size;
        ends_.push_back(base + at);
        file_.push_back(file);
    }
}

std::vector<std::uint8_t> PacedUnits::take(std::size_t& allowance, int& files) {
    std::vector<std::uint8_t> out;
    const std::size_t start = next_ == 0 ? 0 : ends_[next_ - 1];
    std::size_t end = start;
    while (next_ < ends_.size() && ends_[next_] - end <= allowance && (!file_[next_] || files > 0)) {
        if (file_[next_]) --files;
        allowance -= ends_[next_] - end;
        end = ends_[next_++];
    }
    out.assign(bytes_.begin() + static_cast<long>(start), bytes_.begin() + static_cast<long>(end));
    return out;
}

void PacedUnits::drop_batch() {
    if (tail_unit_ == static_cast<std::size_t>(-1) || tail_unit_ >= ends_.size()) {
        next_ = ends_.size();
        return;
    }
    if (next_ >= tail_unit_) return; // the batch is out: only the tail is left
    // Keep the tail: copy it down over the unsent rest of the batch.
    const std::size_t from = ends_[tail_unit_ - 1]; // tail_unit_ > next_ >= 0
    const std::size_t start = next_ == 0 ? 0 : ends_[next_ - 1];
    std::vector<std::uint8_t> tail(bytes_.begin() + static_cast<long>(from), bytes_.end());
    std::vector<std::size_t> tail_ends;
    std::vector<bool> tail_files;
    for (std::size_t u = tail_unit_; u < ends_.size(); ++u) {
        tail_ends.push_back(ends_[u] - from + start);
        tail_files.push_back(file_[u]);
    }
    bytes_.resize(start);
    bytes_.insert(bytes_.end(), tail.begin(), tail.end());
    ends_.resize(next_);
    ends_.insert(ends_.end(), tail_ends.begin(), tail_ends.end());
    file_.resize(next_);
    file_.insert(file_.end(), tail_files.begin(), tail_files.end());
    tail_unit_ = next_;
}

} // namespace nnh::tool::reload
