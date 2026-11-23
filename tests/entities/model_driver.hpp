#pragma once
// Drives an nnh::World and the reference model (world_model.hpp) with the same operations and compares them after each
// one (spec 020 T3.4, T4.3, T4.4). Kinds MA, MB, MC carry an int; every kind has a remove hook that, depending on the
// value, destroys another entity (queued) or spawns one with an MA — the same decision in the world and the model.

#include "nnh/entities.hpp"
#include "world_model.hpp"

#include <cstdint>

namespace model_test {

struct MA {
    int v = 0;
};
struct MB {
    int v = 0;
};
struct MC {
    int v = 0;
};

inline constexpr int kEntities = 128;
inline constexpr int kLiveLimit = 32;
inline constexpr std::uint32_t kTags[4] = {0x1111u, 0x2222u, 0x3333u, 0x4444u};

class Driver {
public:
    explicit Driver(int tags);
    ~Driver();

    /// One operation chosen by `op` and `arg` (any values). Returns false on the first mismatch.
    bool step(std::uint32_t op, std::uint32_t arg);
    /// Compares every entity, component and tag of the world and the model.
    bool same() const;
    /// FNV-1a of everything observed (handles, visits, find results): equal runs give equal sums.
    std::uint32_t sum() const { return sum_; }
    const char* mismatch() const { return mismatch_; }

    // The hooks' shared decision, called by the world's hooks and the model's.
    void world_hook(int kind, int slot, int value);
    static void model_hook(world_model::Model& m, int kind, int slot, int value);

private:
    bool fail(const char* what) {
        if (mismatch_ == nullptr) mismatch_ = what;
        return false;
    }
    void mix(std::uint32_t word);
    std::uint32_t random_handle_world(std::uint32_t arg) const;
    bool compare_each(std::uint32_t op, std::uint32_t arg);
    bool compare_tags(std::uint32_t arg);

    nnh::World world_;
    world_model::Model model_;
    nnh::Entity handles_[kEntities] = {}; ///< the world's last handle per slot (hooks destroy by slot)
    std::uint32_t sum_ = 2166136261u;
    const char* mismatch_ = nullptr;
};

} // namespace model_test
