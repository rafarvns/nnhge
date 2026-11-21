#include "remote/stdin_lines.hpp"

#include <deque>
#include <mutex>
#include <thread>

namespace nnh::tool {

/// What the reading thread and the session share; the thread keeps it alive after StdinLines is gone.
struct StdinLines::Shared {
    std::mutex mutex;
    std::deque<std::string> lines;
    bool at_end = false;
};

namespace {

void read_lines(std::FILE* in, const std::shared_ptr<StdinLines::Shared>& shared);

} // namespace

StdinLines::StdinLines() : shared_(std::make_shared<Shared>()) {}
StdinLines::~StdinLines() = default;

void StdinLines::start(std::FILE* in) {
    std::thread(read_lines, in, shared_).detach();
}

bool StdinLines::take(std::string& line) {
    std::lock_guard<std::mutex> lock(shared_->mutex);
    if (shared_->lines.empty()) return false;
    line = std::move(shared_->lines.front());
    shared_->lines.pop_front();
    return true;
}

bool StdinLines::ended() const {
    std::lock_guard<std::mutex> lock(shared_->mutex);
    return shared_->at_end && shared_->lines.empty();
}

namespace {

void push(StdinLines::Shared& shared, std::string& line) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    std::lock_guard<std::mutex> lock(shared.mutex);
    shared.lines.push_back(std::move(line));
    line.clear();
}

void read_lines(std::FILE* in, const std::shared_ptr<StdinLines::Shared>& shared) {
    std::string line;
    char buffer[1024];
    while (std::fgets(buffer, sizeof buffer, in) != nullptr) {
        line += buffer;
        if (!line.empty() && line.back() == '\n') push(*shared, line);
    }
    if (!line.empty()) push(*shared, line); // a last line without '\n'
    std::lock_guard<std::mutex> lock(shared->mutex);
    shared->at_end = true;
}

} // namespace

} // namespace nnh::tool
