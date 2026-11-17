// Nintendo 3DS platform: application lifecycle — GPU setup, APT events and, in Debug/Development builds
// launched with 3dslink, stdout redirection to the PC (libctru + citro3d/citro2d).

#include <3ds.h>
#include <arpa/inet.h>
#include <citro2d.h>
#include <malloc.h>
#include <unistd.h>

#include "context_3ds.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/app.hpp"

namespace nnh::platform {
namespace {

constexpr std::size_t kDebugTextGlyphs = 4096;
constexpr std::size_t kSocAlign = 0x1000;
constexpr std::size_t kSocBufferSize = 0x100000; // 1 MB, only in Debug/Development when 3dslink is used

ctr::Context g_context;
const char* g_last_error = "";
u32* g_soc_buffer = nullptr;
int g_link_socket = -1;

/// When the Homebrew Launcher started us through 3dslink, send stdout/stderr to the PC
/// (the PC must run `3dslink -s ...` to keep listening). Silently does nothing otherwise.
void start_3dslink_stdio() {
    if constexpr (!build::kDevToolsEnabled) return;
    if (__3dslink_host.s_addr == 0) return;
    g_soc_buffer = static_cast<u32*>(memalign(kSocAlign, kSocBufferSize));
    if (g_soc_buffer == nullptr) return;
    if (R_FAILED(socInit(g_soc_buffer, kSocBufferSize))) {
        free(g_soc_buffer);
        g_soc_buffer = nullptr;
        return;
    }
    g_link_socket = link3dsStdio();
}

void stop_3dslink_stdio() {
    if (g_link_socket >= 0) close(g_link_socket);
    g_link_socket = -1;
    if (g_soc_buffer != nullptr) {
        socExit();
        free(g_soc_buffer);
        g_soc_buffer = nullptr;
    }
}

} // namespace

ctr::Context& ctr::context() { return g_context; }

bool init(const AppConfig& config) {
    (void)config; // title and icon come from the SMDH on 3DS
    g_last_error = "";
    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        g_last_error = "C3D_Init failed";
        gfxExit();
        return false;
    }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        g_last_error = "C2D_Init failed";
        C3D_Fini();
        gfxExit();
        return false;
    }
    C2D_Prepare();
    g_context.targets[0] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_context.targets[1] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_context.text_buffer = C2D_TextBufNew(kDebugTextGlyphs);
    start_3dslink_stdio();
    return true;
}

void shutdown() {
    stop_3dslink_stdio();
    C2D_TextBufDelete(g_context.text_buffer);
    g_context = {};
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

const char* last_error() { return g_last_error; }

bool poll_events() { return aptMainLoop(); }

} // namespace nnh::platform
