# NNH Link

The console app of [spec 011](../../specs/011-nnh-link/spec.md): pick a PC running `nnh serve`, pair
once with the code it shows, pick a game; the PC builds it and sends it, and the console starts it (and
comes back here when the game exits). Made with NNHGE: scenes, text, input, `nnh::save`, plus the
`nnh_link` library and the 3DS extensions `<nnh/3ds/network.hpp>` and `<nnh/3ds/launch.hpp>`.

- Build: with the other games (`build/3ds-*/apps/nnh-link/nnh-link.3dsx`); copy it to `sdmc:/3ds/` and
  open it from the Homebrew Launcher. Its data and the received games go to `sdmc:/3ds/nnh/`.
- PC simulator: everything but starting the game. `NNH_LINK_DEMO=servers|address|code|games|building|receiving|failed|done`
  shows a made-up server (for screenshots).
- Tests: `build/pc-debug/apps/nnh-link/nnh_link_app_tests` (the logic, storage, pad, layout and file
  writing, against fakes).
- How it is organized: [plan 011](../../specs/011-nnh-link/plan.md) ("NNH Link"); server side:
  [nnh-tool.md](../../docs/guides/nnh-tool.md).
