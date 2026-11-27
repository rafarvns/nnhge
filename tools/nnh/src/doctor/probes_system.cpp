// The real probes of nnh doctor (probes.hpp): this machine's environment, files, programs, disk and network.

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>

#include "console_link.hpp"
#include "doctor/probes.hpp"
#include "host_compiler.hpp"
#include "netloader.hpp"
#include "text_file.hpp"
#include "user_config.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace nnh::tool {
namespace {

namespace fs = std::filesystem;

fs::path u8path(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }

std::string os_name() {
#if defined(_WIN32)
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof v;
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto get = ntdll ? reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")))
                           : nullptr;
    if (!get || get(&v) != 0) return "Windows";
    const std::string number = std::to_string(v.dwMajorVersion) + "." + std::to_string(v.dwMinorVersion) + "." +
                               std::to_string(v.dwBuildNumber);
    const char* name = v.dwMajorVersion == 10 ? (v.dwBuildNumber >= 22000 ? "Windows 11" : "Windows 10") : "Windows";
    return std::string(name) + " (" + number + ")";
#elif defined(__APPLE__)
    return "macOS";
#else
    const std::string text = read_text_file("/etc/os-release", 16 * 1024);
    const std::size_t at = text.find("PRETTY_NAME=\"");
    if (at == std::string::npos) return "Linux";
    const std::size_t start = at + 13, end = text.find('"', start);
    return end == std::string::npos ? "Linux" : text.substr(start, end - start);
#endif
}

std::string find_console(const std::string& console) {
    std::vector<Ipv4> targets;
    if (!console.empty()) {
        if (const auto given = parse_ipv4(console)) targets.push_back(*given);
    } else {
        const Settings settings = load_user_settings(); // read only: doctor writes nothing
        const auto remembered = settings.find("console");
        if (remembered != settings.end()) {
            if (const auto last = parse_ipv4(remembered->second); last && is_local_network(*last)) targets.push_back(*last);
        }
        for (const Ipv4& b : net::local_broadcast_addresses()) targets.push_back(b);
        targets.push_back(Ipv4{{255, 255, 255, 255}});
    }
    const auto found = discover_console(targets, kNetloaderPort, kNetloaderPort, 3, 1000);
    return found && is_local_network(*found) ? to_string(*found) : "";
}

/// One byte of `url` through `cmake -P` on a script made for it in the system's temporary folder (removed after).
std::string fetch(const std::string& cmake, const std::string& url, const std::string& cainfo) {
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec);
    if (ec) return "no temporary folder";
    std::random_device random;
    const fs::path script = dir / ("nnh-doctor-" + std::to_string(random()) + ".cmake");
    {
        std::ofstream out(script, std::ios::binary);
        out << "file(DOWNLOAD \"" << url << "\" RANGE_START 0 RANGE_END 0 STATUS st TIMEOUT 8 INACTIVITY_TIMEOUT 8";
        if (!cainfo.empty()) out << " TLS_CAINFO \"" << cainfo << "\"";
        out << ")\nmessage(\"nnh-status: ${st}\")\n";
        if (!out) return "could not write a temporary script";
    }
    const RunResult r = run_program(cmake, {"-P", script.string()}, kDoctorProgramTimeoutMs, kDoctorMaxOutput);
    fs::remove(script, ec);
    if (!r.started) return "cmake did not start";
    if (r.timed_out) return "no answer in 10 s";
    const std::size_t at = r.output.find("nnh-status: ");
    if (at == std::string::npos) return "cmake gave no status";
    const std::string status = r.output.substr(at + 12, r.output.find('\n', at) - at - 12);
    return status.rfind("0;", 0) == 0 ? "" : status;
}

} // namespace

Probes system_probes() {
    Probes p;
    p.env = [](const char* name) {
        const char* v = std::getenv(name);
        return std::string(v ? v : "");
    };
    p.is_file = [](const std::string& path) {
        std::error_code ec;
        return fs::is_regular_file(u8path(path), ec);
    };
    p.is_dir = [](const std::string& path) {
        std::error_code ec;
        return fs::is_directory(u8path(path), ec);
    };
    p.read_file = [](const std::string& path, std::size_t limit) { return read_text_file(path, limit); };
    p.list_dir = [](const std::string& dir) {
        std::vector<std::string> names;
        std::error_code ec;
        for (fs::directory_iterator it(u8path(dir), ec), end; !ec && it != end; it.increment(ec)) {
            const std::u8string n = it->path().filename().u8string();
            names.emplace_back(n.begin(), n.end());
        }
        std::sort(names.begin(), names.end());
        return names;
    };
    p.find_program = [](const char* name) { return find_on_path(name); };
    p.run = [](const std::string& program, const std::vector<std::string>& args) {
        return run_program(program, args, kDoctorProgramTimeoutMs, kDoctorMaxOutput);
    };
    p.free_bytes = [](const std::string& path) -> std::int64_t {
        std::error_code ec;
        const fs::space_info s = fs::space(u8path(path), ec);
        return ec ? -1 : static_cast<std::int64_t>(s.available);
    };
    p.os_name = os_name;
    p.find_console = find_console;
    p.fetch = fetch;
    return p;
}

} // namespace nnh::tool
