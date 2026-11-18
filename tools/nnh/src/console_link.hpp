#pragma once
// Talking to the console (spec 010): finding the Homebrew Launcher's netloader, sending it a .3dsx, and
// sending .cia URLs to FBI. Ports are parameters so the loopback tests can use free ones; nnh passes the
// real ones (17491 and 5000). Tested in tools/nnh/tests/test_console_link.cpp.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "address.hpp"

namespace nnh::tool {

/// Broadcasts "3dsboot" to `target`:`port` and waits for "boot3ds", which the netloader sends back to
/// this computer's `reply_port` (17491, whatever port asked). Tries `tries` times, `wait_ms` each.
/// @return the console's address, or nullopt if none answered.
std::optional<Ipv4> discover_console(Ipv4 target, std::uint16_t port, std::uint16_t reply_port, int tries, int wait_ms);

/// Sends `file` as `name` (saved as sdmc:/3ds/<name>, argv[0]) and starts it with `extra_args`.
/// @return "" on success, else what went wrong.
std::string send_3dsx(Ipv4 console, std::uint16_t port, const std::string& name, const std::vector<std::uint8_t>& file,
                      const std::vector<std::string>& extra_args, int connect_timeout_ms = 5000);

/// Sends URLs to FBI's "Receive URLs over the network" and waits up to `timeout_ms` for it to finish
/// (it answers or closes). @return "" on success, else what went wrong.
std::string fbi_send_urls(Ipv4 console, std::uint16_t port, const std::vector<std::string>& urls, int timeout_ms,
                          int connect_timeout_ms = 5000);

} // namespace nnh::tool
