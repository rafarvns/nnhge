// Nintendo 3DS platform: application lifecycle — GPU setup, APT events and, in Debug/Development builds
// launched with 3dslink, stdout redirection to the PC (libctru + citro3d/citro2d).

#include <3ds.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <citro2d.h>
#include <malloc.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>

#include "context_3ds.hpp"
#include "nnh/3ds/network.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/app.hpp"
#include "stdio_fs.hpp"

namespace nnh::platform {
namespace {

constexpr std::size_t kDebugTextGlyphs = 4096; // core: kDebugPassCapacity

ctr::Context g_context;
const char* g_last_error = "";
bool g_log_network = false; // the log redirect holds one network_start() (Debug/Development with 3dslink)
int g_link_socket = -1;

constexpr int kLinkTimeoutMs = 1000;

/// Connects to the PC that sent us with 3dslink, giving up after kLinkTimeoutMs. libctru's
/// link3dsConnectToHost() uses a blocking connect(): when the PC isn't listening (`3dslink` without
/// `-s`) and its firewall drops the attempt instead of refusing it, the console waits forever on a
/// black screen. @return the connected socket (blocking again), or -1.
int connect_to_3dslink_host() {
    const int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return -1;
    const int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr = __3dslink_host;
    address.sin_port = htons(LINK3DS_COMM_PORT);
    if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
        // Connected = writable without an error or hang-up. (getsockopt(SO_ERROR) is not reliable on the
        // 3DS's socket service: on hardware it reported a failure for a good connection.)
        pollfd ready{sock, POLLOUT, 0};
        const bool connected = errno == EINPROGRESS && poll(&ready, 1, kLinkTimeoutMs) > 0 &&
                               (ready.revents & POLLOUT) != 0 && (ready.revents & (POLLERR | POLLHUP)) == 0;
        if (!connected) {
            close(sock);
            return -1;
        }
    }
    fcntl(sock, F_SETFL, flags);
    return sock;
}

/// When the Homebrew Launcher started us through 3dslink, send stdout/stderr to the PC
/// (the PC must run `3dslink -s ...` to keep listening). Without a listening PC the game starts after at
/// most kLinkTimeoutMs, without logs.
void start_3dslink_stdio() {
    if constexpr (!build::kDevToolsEnabled) return;
    if (__3dslink_host.s_addr == 0) return;
    if (!n3ds::network_start()) return;
    g_log_network = true;
    g_link_socket = connect_to_3dslink_host();
    if (g_link_socket < 0) {
        n3ds::network_stop();
        g_log_network = false;
        return;
    }
    dup2(g_link_socket, STDOUT_FILENO);
    dup2(g_link_socket, STDERR_FILENO);
}

/// The socket is dup2()ed onto stdout and stderr: those descriptors must be closed too before
/// socExit(), or newlib's exit-time flush/close of stdio talks to a dead SOC service and the console
/// hangs on a black screen instead of returning to the Homebrew Launcher.
void stop_3dslink_stdio() {
    if (g_link_socket >= 0) {
        std::fflush(stdout);
        std::fflush(stderr);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        close(g_link_socket);
    }
    g_link_socket = -1;
    if (g_log_network) n3ds::network_stop();
    g_log_network = false;
}

} // namespace

ctr::Context& ctr::context() { return g_context; }
void ctr::set_error(const char* what) { g_last_error = what; }
int ctr::link_socket() { return g_link_socket; }

bool init(const AppConfig& config) {
    // The window title and icon come from the SMDH on 3DS; config gives the data folder.
    g_last_error = "";
    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        g_last_error = "C3D_Init failed";
        gfxExit();
        return false;
    }
    // One citro2d object per sprite or shape (outlines take 4), one per tile quad (core caps them at
    // max_tiles per frame, spec 012), one per particle quad (max_particles, spec 024), plus the debug pass (debug text
    // glyphs and overlay rectangles, core's kDebugPassCapacity = 4096) and some headroom.
    const int max_draws = config.gfx.max_draws > 0 ? config.gfx.max_draws : 0;
    const int max_tiles = config.gfx.max_tiles > 0 ? config.gfx.max_tiles : 0;
    const int max_particles = config.gfx.max_particles > 0 ? config.gfx.max_particles : 0;
    const std::size_t max_objects = static_cast<std::size_t>(max_draws) + static_cast<std::size_t>(max_tiles) +
                                    static_cast<std::size_t>(max_particles) + kDebugTextGlyphs + 256;
    if (!C2D_Init(max_objects > C2D_DEFAULT_MAX_OBJECTS ? max_objects : C2D_DEFAULT_MAX_OBJECTS)) {
        g_last_error = "C2D_Init failed";
        C3D_Fini();
        gfxExit();
        return false;
    }
    C2D_Prepare();
    g_context.targets[0] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_context.targets[1] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    ctr::input_start();
    start_3dslink_stdio();
    char root[96];
    std::snprintf(root, sizeof root, "sdmc:/3ds/%s/", config.data_folder);
    shared::stdio_fs_set_root(root);
    return true;
}

void shutdown() {
    stop_3dslink_stdio();
    ctr::network_release_all(); // sockets the game left open
    ctr::return_if_asked();     // started by NNH Link: it starts again when we exit
    ctr::input_stop();
    g_context = {};
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

const char* last_error() { return g_last_error; }

bool poll_events() { return aptMainLoop(); }

} // namespace nnh::platform
