// Probe pixels (nnh/gfx3d.hpp request_probes, FR-24, plan P19). Stage 4: the API exists and answers nothing; the
// read-back through the remote capture contract lands with the console backend (stage 7).

#include "gfx3d/gfx3d_state.hpp"

namespace nnh::gfx3d {

bool request_probes(Screen, Eye, const Probe*, int) { return false; }

int probe_results(Color*, int) { return 0; }

} // namespace nnh::gfx3d
