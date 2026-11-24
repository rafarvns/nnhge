// The migration step runner (spec 027, D46, FR-12): steps v, v + 1, ... c - 1, in order, after the automatic
// load.

#include "nnh/assert.hpp"
#include "saves/save_internal.hpp"

namespace nnh::detail {

int save_run_steps(const SaveTypeInfo& type, void* value, std::uint32_t from, const save::OldFields& old) {
    int run = 0;
    for (std::uint32_t version = from; version < type.version; ++version) {
        // The chain is complete at compile time (step i converts from version i + 1); found by its index.
        const std::uint32_t index = version - 1;
        const SaveStep* step =
            index < type.step_count && type.steps[index].from == version ? &type.steps[index] : nullptr;
        NNH_ASSERT(step != nullptr && step->run != nullptr, "save '%s': no migration step from version %lu",
                   type.key, static_cast<unsigned long>(version));
        if (step == nullptr || step->run == nullptr)
            continue; // Release: a no-op step, never undefined behaviour
        step->run(value, old);
        ++run;
    }
    return run;
}

} // namespace nnh::detail
