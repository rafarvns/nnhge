// Nintendo 3DS platform: starting a .3dsx after exit through Luma3DS's `hb:ldr` (<nnh/3ds/launch.hpp>), and
// NNH Link's "--nnh-return=<path>" at shutdown. Argument rules and sources: hbldr_args.hpp.

#include <3ds.h>

#include <cstring>

#include "context_3ds.hpp"
#include "hbldr_args.hpp"
#include "nnh/3ds/launch.hpp"

extern "C" {
extern int __system_argc;     // libctru (initArgv.c), the 3dslink marker already removed
extern char** __system_argv;
}

namespace nnh {
namespace {

using platform::ctr::HbldrArgv;

HbldrArgv g_argv;       // 1 KB: kept off the stack
bool g_launch_set = false; // the game arranged its own launch: no return to NNH Link

Result hbldr_set_target(Handle loader, const char* target) {
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(2, 0, 2); // SetTarget
    cmdbuf[1] = IPC_Desc_StaticBuffer(std::strlen(target) + 1, 0);
    cmdbuf[2] = reinterpret_cast<u32>(target);
    const Result rc = svcSendSyncRequest(loader);
    return R_SUCCEEDED(rc) ? static_cast<Result>(cmdbuf[1]) : rc;
}

Result hbldr_set_argv(Handle loader, const HbldrArgv& argv) {
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(3, 0, 2); // SetArgv
    cmdbuf[1] = IPC_Desc_StaticBuffer(sizeof argv, 1);
    cmdbuf[2] = reinterpret_cast<u32>(argv.words);
    const Result rc = svcSendSyncRequest(loader);
    return R_SUCCEEDED(rc) ? static_cast<Result>(cmdbuf[1]) : rc;
}

bool started_by_homebrew_loader() { return (envGetSystemRunFlags() & RUNFLAG_APTCHAINLOAD) != 0; }

} // namespace

bool n3ds::can_launch() {
    if (!started_by_homebrew_loader()) return false;
    Handle loader = 0;
    if (R_FAILED(svcConnectToPort(&loader, "hb:ldr"))) return false;
    svcCloseHandle(loader);
    return true;
}

bool n3ds::launch_after_exit(const char* path, const char* const* args, int count) {
    // On exit, libctru jumps back to this title (the run flag Luma sets) and the loader loads the target.
    if (!started_by_homebrew_loader()) return false;
    const char* target = platform::ctr::hbldr_target(path);
    if (target == nullptr || !platform::ctr::build_hbldr_argv(path, args, count, g_argv)) return false;
    Handle loader = 0;
    if (R_FAILED(svcConnectToPort(&loader, "hb:ldr"))) return false;
    // Argv first: if setting the target fails, the next load is the Homebrew Launcher, which ignores argv.
    const bool ok = R_SUCCEEDED(hbldr_set_argv(loader, g_argv)) && R_SUCCEEDED(hbldr_set_target(loader, target));
    svcCloseHandle(loader);
    g_launch_set = g_launch_set || ok;
    return ok;
}

void platform::ctr::return_if_asked() {
    if (g_launch_set) return;
    const char* path = return_path(__system_argc, __system_argv);
    if (path != nullptr) n3ds::launch_after_exit(path);
}

} // namespace nnh
