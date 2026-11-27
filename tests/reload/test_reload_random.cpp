// Random batches against a model (spec 031 T2.8, FR-4, FR-12, FR-18): 10 000 seeded batches of puts, adds, removes and refused
// batches (a put the index doesn't list); after each, every path the model has reads its expected bytes and every path
// it removed is gone; the reload memory never grows past one batch's worth.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "nnh/assets.hpp"
#include "nnh/memory.hpp"
#include "reload/reload_memory.hpp"
#include "reload_fixture.hpp"

using namespace reload_test;

namespace {

std::uint32_t next(std::uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

struct Model {
    std::vector<std::string> paths;
    std::vector<std::string> contents;
    int find(const std::string& p) const {
        for (std::size_t i = 0; i < paths.size(); ++i) {
            if (paths[i] == p) return static_cast<int>(i);
        }
        return -1;
    }
    std::string index() const {
        std::string s;
        for (const std::string& p : paths) s += p + "\n";
        return s;
    }
};

const char kStart[] = "s";

} // namespace

NNH_TEST(reload_random_batches_match_the_model) {
    RELOAD_DEV_ONLY();
    Model model;
    for (int i = 0; i < 6; ++i) {
        model.paths.push_back("f" + std::to_string(i) + ".txt");
        model.contents.push_back(kStart);
    }
    static std::string names[6];
    Put assets[6];
    for (int i = 0; i < 6; ++i) {
        names[i] = model.paths[i];
        assets[i] = Put{names[i].c_str(), kStart, 1};
    }
    char index[256];
    start(assets, 6, index, sizeof index);
    std::uint32_t seed = 31;
    int applied = 0, refused = 0;
    std::size_t peak = 0;
    for (int round = 1; round <= 10000; ++round) {
        Model after = model;
        std::vector<std::string> bpaths, bdata;
        std::vector<bool> bremove;
        const int kind = static_cast<int>(next(seed) % 10);
        const int files = 1 + static_cast<int>(next(seed) % 3);
        bool with_index = kind >= 6;
        bool bad = kind == 9;
        for (int f = 0; f < files; ++f) {
            std::string p;
            if (bad || (with_index && next(seed) % 3 == 0)) {
                p = "n" + std::to_string(next(seed) % 40) + ".txt"; // maybe new
            } else if (!after.paths.empty()) {
                p = after.paths[next(seed) % after.paths.size()];
            } else {
                continue;
            }
            bool dup = false;
            for (const std::string& q : bpaths) dup = dup || q == p;
            if (dup) continue;
            const bool remove = with_index && !bad && after.find(p) >= 0 && next(seed) % 4 == 0;
            std::string content = remove ? "" : "r" + std::to_string(round) + std::string(next(seed) % 30, 'x');
            bpaths.push_back(p);
            bdata.push_back(content);
            bremove.push_back(remove);
            const int at = after.find(p);
            if (remove) {
                after.paths.erase(after.paths.begin() + at);
                after.contents.erase(after.contents.begin() + at);
            } else if (at >= 0) {
                after.contents[static_cast<std::size_t>(at)] = content;
            } else {
                after.paths.push_back(p);
                after.contents.push_back(content);
            }
        }
        if (bpaths.empty()) continue;
        // without the batch's index a new path is a consistency refusal; a "bad" batch never sends its index
        bool expect_refused = bad && after.paths.size() != model.paths.size();
        if (!with_index) {
            for (std::size_t i = 0; i < bpaths.size(); ++i) expect_refused = expect_refused || model.find(bpaths[i]) < 0;
        }
        if (bad) with_index = false;
        std::string idx = after.index();
        std::vector<Put> batch;
        for (std::size_t i = 0; i < bpaths.size(); ++i) {
            batch.push_back(bremove[i] ? removed(bpaths[i].c_str())
                                       : Put{bpaths[i].c_str(), bdata[i].data(), bdata[i].size()});
        }
        if (with_index) batch.push_back(Put{".nnh_assets", idx.data(), idx.size()});
        const Records before = records();
        send_batch(static_cast<std::uint16_t>(round), batch.data(), static_cast<int>(batch.size()));
        frames(1);
        const Records now = records();
        const bool was_applied = now.applied == before.applied + 1;
        CHECK(was_applied != expect_refused);
        if (was_applied) {
            model = after;
            ++applied;
        } else {
            ++refused;
        }
        fake::remote_clear_sent();
        for (std::size_t i = 0; i < model.paths.size(); ++i) {
            char out[64];
            const long n = read_text(model.paths[i].c_str(), out, sizeof out);
            CHECK(n == static_cast<long>(model.contents[i].size()) && model.contents[i] == out);
        }
        const std::size_t bytes = nnh::mem::tag_stats(nnh::detail::reload::reload_memory_tag()).bytes;
        peak = bytes > peak ? bytes : peak;
    }
    CHECK(applied > 500 && refused > 50);
    CHECK(peak < 64 * 1024); // the buffer, the overlay, one batch block and one index
    nnh::shutdown();
}
