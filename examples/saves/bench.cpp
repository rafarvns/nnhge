// The saves example's bench workloads (bench.hpp).

#include "bench.hpp"

#include "reference.hpp"
#include "slot_files.hpp"

#include <nnh/fs.hpp>

namespace saves_example {
namespace {

constexpr const char* kStore = "bench.sav";
constexpr const char* kBackup = "bench_backup.sav";
constexpr const char* kRaw = "bench_raw.sav";
constexpr const char* kBenchSlots[3] = {"bench_slot1.sav", "bench_slot2.sav", "bench_slot3.sav"};

} // namespace

bool SavesBench::start() {
    fill_reference_v3(value_);
    fill_reference_v1(v1_);
    const auto v3 = nnh::save::encode(value_, v3_payload_, sizeof v3_payload_);
    const auto v1 = nnh::save::encode(v1_, v1_payload_, sizeof v1_payload_);
    if (!v3 || !v1) return false;
    v3_size_ = v3.value();
    v1_size_ = v1.value();
    return true;
}

void SavesBench::finish() {
    for (const char* f : {kStore, kBackup, kRaw, kBenchSlots[0], kBenchSlots[1], kBenchSlots[2]})
        (void)nnh::save::remove(f);
}

bool SavesBench::encode() {
    const auto n = nnh::save::encode(value_, buffer_.bytes, sizeof buffer_.bytes);
    sink_ += n ? static_cast<std::uint32_t>(n.value()) + buffer_.bytes[n.value() - 1] : 0;
    return n.ok();
}

bool SavesBench::decode() {
    const auto r = nnh::save::decode(v3_payload_, v3_size_, loaded_);
    sink_ += loaded_.gems;
    return r.ok();
}

bool SavesBench::decode_v1() {
    const auto r = nnh::save::decode(v1_payload_, v1_size_, loaded_);
    sink_ += loaded_.gems;
    return r.ok() && r.value().migrated == 2;
}

bool SavesBench::store() {
    return nnh::save::store(kStore, value_, buffer_).ok();
}

bool SavesBench::load() {
    const auto r = nnh::save::load(kStore, loaded_, buffer_);
    sink_ += loaded_.gems;
    return r.ok();
}

bool SavesBench::prepare_backup() {
    return nnh::save::store(kBackup, value_, buffer_) && nnh::save::store(kBackup, value_, buffer_) &&
           damage_save(kBackup, buffer_.bytes, sizeof buffer_.bytes);
}

bool SavesBench::load_backup() {
    const auto r = nnh::save::load(kBackup, loaded_, buffer_);
    return r.ok() && r.value().used_backup;
}

bool SavesBench::prepare_summaries() {
    for (const char* f : kBenchSlots) {
        if (!nnh::save::store(f, value_, buffer_)) return false;
    }
    return true;
}

bool SavesBench::summaries() {
    bool ok = true;
    for (const char* f : kBenchSlots) {
        ok = nnh::save::load(f, summary_, buffer_.bytes, sizeof buffer_.bytes).ok() && ok;
        sink_ += summary_.level;
    }
    return ok;
}

bool SavesBench::raw_write() {
    return nnh::save::write(kRaw, v3_payload_, v3_size_, 1).ok();
}

} // namespace saves_example
