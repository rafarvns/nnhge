// Mini-fuzz of structured saves (spec 027 T6.3, AC-4): the three golden payloads and two seeds of the fuzz target's
// reference type (every kind and shape), with 1-4 bytes changed and sometimes cut, 3000 mutants each, through the
// target (payload as given, fields behind a valid header, a whole file through load on the fake). Accepted mutants
// are counted to prove the deep paths run. The real fuzzing (libFuzzer + ASan, fuzz_saves) runs on Linux.

#include <cstdio>
#include <string>
#include <vector>

#include "fs/save_format.hpp"
#include "fuzz_saves.hpp"
#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace {

using Bytes = std::vector<std::uint8_t>;

std::uint32_t next(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
}

Bytes golden_payload(int version) {
    Bytes file;
    std::FILE* f = std::fopen((std::string(NNH_TEST_DATA_DIR) + "/saves/v" + std::to_string(version) + ".sav").c_str(),
                              "rb");
    if (f == nullptr) return file;
    std::uint8_t chunk[1024];
    std::size_t n = 0;
    while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0) file.insert(file.end(), chunk, chunk + n);
    std::fclose(f);
    return file.size() > 16 ? Bytes(file.begin() + 16, file.end()) : Bytes{};
}

Bytes seed(bool full) {
    Bytes b(8192);
    b.resize(nnh::fuzz::saves_seed(full, b.data(), b.size()));
    return b;
}

} // namespace

NNH_TEST(saves_target_survives_mutated_inputs) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init());
    const Bytes seeds[] = {seed(true), seed(false), golden_payload(1), golden_payload(2), golden_payload(3)};
    std::uint32_t state = 2700;
    int accepted = 0;
    for (const Bytes& s : seeds) {
        CHECK(!s.empty());
        nnh::fuzz::saves_target(s.data(), s.size());
        for (int i = 0; i < 3000; ++i) {
            Bytes b = s;
            if (i % 8 == 0) b.resize(next(state) % (s.size() + 1));
            const int flips = 1 + static_cast<int>(next(state) % 4);
            for (int k = 0; k < flips && !b.empty(); ++k) {
                b[next(state) % b.size()] ^= static_cast<std::uint8_t>(1 + next(state) % 255);
            }
            nnh::fuzz::saves_target(b.data(), b.size());
            accepted += nnh::fuzz::saves_accepted(b.data(), b.size()) ? 1 : 0;
        }
    }
    std::printf("    saves mini-fuzz: %d of 15000 mutants accepted\n", accepted);
    CHECK(accepted > 1000);
    nnh::fuzz::saves_target(nullptr, 0);
    nnh::shutdown();
}

NNH_TEST(saves_target_accepts_its_seeds) {
    CHECK(nnh::fuzz::saves_accepted(seed(true).data(), seed(true).size()));
    CHECK(nnh::fuzz::saves_accepted(seed(false).data(), seed(false).size()));
}
