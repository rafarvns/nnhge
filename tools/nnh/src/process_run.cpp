// Running a program with a time limit and bounded output (process_run.hpp).

#include "process_run.hpp"

#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace nnh::tool {
namespace {

using Clock = std::chrono::steady_clock;

int ms_left(Clock::time_point deadline) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    return left > 0 ? static_cast<int>(left) : 0;
}

void keep(std::string& output, const char* data, std::size_t n, std::size_t max_output) {
    if (output.size() < max_output) output.append(data, std::min(n, max_output - output.size()));
}

#ifdef _WIN32
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

RunResult run(const std::string& program, const std::vector<std::string>& args, int timeout_ms,
              std::size_t max_output) {
    RunResult r;
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE read_end = nullptr, write_end = nullptr;
    if (!CreatePipe(&read_end, &write_end, &sa, 0)) return r;
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul =
        CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof si;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = nul;
    si.StartupInfo.hStdOutput = write_end;
    si.StartupInfo.hStdError = write_end;
    // Only these two handles are inherited (not every inheritable handle nnh happens to hold).
    HANDLE inherited[2] = {write_end, nul};
    SIZE_T list_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &list_size);
    std::vector<unsigned char> list_bytes(list_size);
    auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(list_bytes.data());
    const bool listed = list_size > 0 && InitializeProcThreadAttributeList(list, 1, 0, &list_size) &&
                        UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                                  nul != INVALID_HANDLE_VALUE ? sizeof inherited : sizeof(HANDLE),
                                                  nullptr, nullptr);
    if (listed) si.lpAttributeList = list;
    PROCESS_INFORMATION pi{};
    std::wstring command = widen(windows_command_line(program, args));
    // A job kills the program and anything it started (bash -lc's children) at the time limit.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job) SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
    const DWORD flags = CREATE_NO_WINDOW | CREATE_SUSPENDED | (listed ? EXTENDED_STARTUPINFO_PRESENT : 0);
    const BOOL created = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, flags, nullptr, nullptr,
                                        &si.StartupInfo, &pi);
    if (listed) DeleteProcThreadAttributeList(list);
    CloseHandle(write_end);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!created) {
        CloseHandle(read_end);
        if (job) CloseHandle(job);
        return r;
    }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    r.started = true;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    char buffer[4096];
    bool exited = false;
    for (;;) {
        if (ms_left(deadline) == 0) { // checked first: a program that never stops writing is still stopped
            if (WaitForSingleObject(pi.hProcess, 0) != WAIT_OBJECT_0) {
                r.timed_out = true;
                TerminateProcess(pi.hProcess, 1);
            }
            if (job) TerminateJobObject(job, 1);
            break;
        }
        DWORD available = 0;
        if (!PeekNamedPipe(read_end, nullptr, 0, nullptr, &available, nullptr)) break; // every writer closed
        if (available > 0) {
            DWORD got = 0;
            if (!ReadFile(read_end, buffer, available < sizeof buffer ? available : sizeof buffer, &got,
                          nullptr))
                break;
            keep(r.output, buffer, got, max_output);
            continue;
        }
        if (exited) break; // exited and nothing left to read (a grandchild may still hold the pipe)
        exited = WaitForSingleObject(pi.hProcess, 5) == WAIT_OBJECT_0;
    }
    WaitForSingleObject(pi.hProcess, 2000);
    DWORD code = 0;
    if (!r.timed_out && GetExitCodeProcess(pi.hProcess, &code)) r.exit_code = static_cast<int>(code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(read_end);
    if (job) CloseHandle(job); // kills what is left of the job
    return r;
}
#else
RunResult run(const std::string& program, const std::vector<std::string>& args, int timeout_ms,
              std::size_t max_output) {
    RunResult r;
    int fds[2];
    if (pipe(fds) != 0) return r;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, fds[1], 1);
    posix_spawn_file_actions_adddup2(&actions, fds[1], 2);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(program.c_str()));
    for (const std::string& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    pid_t pid = 0;
    const int spawned = posix_spawnp(&pid, program.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (spawned != 0) {
        close(fds[0]);
        return r;
    }
    r.started = true;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    char buffer[4096];
    int status = 0;
    bool exited = false;
    for (;;) {
        if (ms_left(deadline) == 0) { // checked first: a program that never stops writing is still stopped
            if (!exited && waitpid(pid, &status, WNOHANG) != pid) {
                r.timed_out = true;
                kill(pid, SIGKILL);
            } else {
                exited = true;
            }
            break;
        }
        pollfd p{fds[0], POLLIN, 0};
        const int ready = poll(&p, 1, exited ? 0 : (ms_left(deadline) < 5 ? ms_left(deadline) : 5));
        if (ready > 0) {
            const ssize_t got = read(fds[0], buffer, sizeof buffer);
            if (got <= 0) break; // every writer closed
            keep(r.output, buffer, static_cast<std::size_t>(got), max_output);
            continue;
        }
        if (ready < 0 && errno == EINTR) continue;
        if (exited) break;
        if (waitpid(pid, &status, WNOHANG) == pid) exited = true;
    }
    if (!exited) waitpid(pid, &status, 0);
    close(fds[0]);
    if (!r.timed_out && WIFEXITED(status)) r.exit_code = WEXITSTATUS(status);
    // A program that could not be executed after the spawn (posix_spawnp reports most failures itself) exits
    // 127.
    if (!r.timed_out && r.exit_code == 127 && r.output.empty()) r.started = false;
    return r;
}
#endif

} // namespace

RunResult run_program(const std::string& program, const std::vector<std::string>& args, int timeout_ms,
                      std::size_t max_output) {
    return run(program, args, timeout_ms, max_output);
}

std::string windows_command_line(const std::string& program, const std::vector<std::string>& args) {
    std::string line = "\"" + program + "\"";
    for (const std::string& arg : args) {
        line += ' ';
        if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos) {
            line += arg;
            continue;
        }
        line += '"';
        std::size_t slashes = 0;
        for (char c : arg) {
            if (c == '\\') {
                ++slashes;
                continue;
            }
            line.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
            slashes = 0;
            line += c;
        }
        line.append(slashes * 2, '\\');
        line += '"';
    }
    return line;
}

} // namespace nnh::tool
