#include "telegram_media/process_runner.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace telegram_media {
namespace {
#ifndef _WIN32
void close_fd(int& fd) { if (fd >= 0) { (void)::close(fd); fd = -1; } }
bool non_blocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
void append_bounded(std::string& out, const char* data, std::size_t size,
                    std::size_t limit, bool& exceeded) {
    const auto room = out.size() >= limit ? 0U : limit - out.size();
    const auto accepted = std::min(room, size);
    out.append(data, accepted);
    exceeded = exceeded || accepted != size;
}
#else
std::wstring widen(const std::string& value) {
    if (value.empty()) return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    (void)::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), out.data(), n);
    return out;
}
std::wstring quote(const std::wstring& value) {
    std::wstring out = L"\"";
    std::size_t slashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') { ++slashes; continue; }
        if (ch == L'"') { out.append(slashes * 2U + 1U, L'\\'); out.push_back(ch); }
        else { out.append(slashes, L'\\'); out.push_back(ch); }
        slashes = 0;
    }
    out.append(slashes * 2U, L'\\'); out.push_back(L'"');
    return out;
}
void read_pipe(HANDLE pipe, std::string& out, std::size_t limit, std::atomic_bool& exceeded) {
    char buffer[8192]; DWORD count = 0;
    while (::ReadFile(pipe, buffer, static_cast<DWORD>(sizeof(buffer)), &count, nullptr) && count) {
        const auto room = out.size() >= limit ? 0U : limit - out.size();
        const auto accepted = std::min(room, static_cast<std::size_t>(count));
        out.append(buffer, accepted);
        if (accepted != static_cast<std::size_t>(count)) exceeded.store(true);
    }
}
#endif
} // namespace

NativeProcessRunner::NativeProcessRunner(std::size_t max_output_bytes)
    : max_output_bytes_(max_output_bytes) {}

ProcessResult NativeProcessRunner::run(const std::vector<std::string>& args,
                                       std::chrono::milliseconds timeout) const {
    ProcessResult result;
    if (args.empty() || args.front().empty()) { result.launch_error = "Program name is required."; return result; }
    if (timeout <= std::chrono::milliseconds::zero()) { result.launch_error = "Timeout must be positive."; return result; }
    if (max_output_bytes_ == 0U) { result.launch_error = "Output limit must be positive."; return result; }
    std::size_t total = 0;
    for (const auto& arg : args) {
        if (arg.find('\0') != std::string::npos || arg.size() > 1024U * 1024U - total) {
            result.launch_error = "Invalid argument vector (NUL byte or over 1 MiB)."; return result;
        }
        total += arg.size();
    }
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE out_r = nullptr, out_w = nullptr, err_r = nullptr, err_w = nullptr;
    if (!::CreatePipe(&out_r, &out_w, &sa, 0) || !::CreatePipe(&err_r, &err_w, &sa, 0)) {
        result.launch_error = "CreatePipe failed: " + std::to_string(::GetLastError());
        if (out_r) ::CloseHandle(out_r); if (out_w) ::CloseHandle(out_w);
        if (err_r) ::CloseHandle(err_r); if (err_w) ::CloseHandle(err_w);
        return result;
    }
    (void)::SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    (void)::SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
    std::wstring command;
    for (const auto& arg : args) {
        const auto wide = widen(arg);
        if (wide.empty() && !arg.empty()) {
            result.launch_error = "Arguments must be valid UTF-8.";
            ::CloseHandle(out_r); ::CloseHandle(out_w); ::CloseHandle(err_r); ::CloseHandle(err_w);
            return result;
        }
        if (!command.empty()) command.push_back(L' ');
        command += quote(wide);
    }
    STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = out_w; si.hStdError = err_w; si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    const BOOL ok = ::CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi);
    ::CloseHandle(out_w); ::CloseHandle(err_w);
    if (!ok) {
        result.launch_error = "CreateProcessW failed: " + std::to_string(::GetLastError());
        ::CloseHandle(out_r); ::CloseHandle(err_r); return result;
    }
    std::atomic_bool out_limit{false}, err_limit{false};
    std::thread out_thread(read_pipe, out_r, std::ref(result.standard_output), max_output_bytes_, std::ref(out_limit));
    std::thread err_thread(read_pipe, err_r, std::ref(result.standard_error), max_output_bytes_, std::ref(err_limit));
    HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        (void)::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
        (void)::AssignProcessToJobObject(job, pi.hProcess);
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    DWORD wait = WAIT_TIMEOUT;
    while (wait == WAIT_TIMEOUT) {
        wait = ::WaitForSingleObject(pi.hProcess, 20U);
        if (out_limit.load() || err_limit.load()) {
            result.output_limit_exceeded = true; (void)::TerminateProcess(pi.hProcess, 1); break;
        }
        if (wait == WAIT_TIMEOUT && std::chrono::steady_clock::now() >= deadline) {
            result.timed_out = true; (void)::TerminateProcess(pi.hProcess, 1); break;
        }
    }
    (void)::WaitForSingleObject(pi.hProcess, INFINITE);
    if (job) { ::CloseHandle(job); job = nullptr; }
    DWORD code = 1;
    if (::GetExitCodeProcess(pi.hProcess, &code)) result.exit_code = static_cast<int>(code);
    out_thread.join(); err_thread.join();
    result.output_limit_exceeded = result.output_limit_exceeded || out_limit.load() || err_limit.load();
    ::CloseHandle(out_r); ::CloseHandle(err_r); ::CloseHandle(pi.hThread); ::CloseHandle(pi.hProcess);
#else
    int out_pipe[2]{-1, -1}, err_pipe[2]{-1, -1};
    if (::pipe(out_pipe) != 0 || ::pipe(err_pipe) != 0) {
        result.launch_error = std::string("pipe failed: ") + std::strerror(errno);
        close_fd(out_pipe[0]); close_fd(out_pipe[1]); close_fd(err_pipe[0]); close_fd(err_pipe[1]); return result;
    }
    const pid_t child = ::fork();
    if (child < 0) {
        result.launch_error = std::string("fork failed: ") + std::strerror(errno);
        close_fd(out_pipe[0]); close_fd(out_pipe[1]); close_fd(err_pipe[0]); close_fd(err_pipe[1]); return result;
    }
    if (child == 0) {
        (void)::setpgid(0, 0); (void)::dup2(out_pipe[1], STDOUT_FILENO); (void)::dup2(err_pipe[1], STDERR_FILENO);
        close_fd(out_pipe[0]); close_fd(out_pipe[1]); close_fd(err_pipe[0]); close_fd(err_pipe[1]);
        std::vector<char*> argv; argv.reserve(args.size() + 1U);
        for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr); ::execvp(argv.front(), argv.data());
        const char message[] = "exec failed\n";
        (void)::write(STDERR_FILENO, message, sizeof(message) - 1U); _exit(127);
    }
    (void)::setpgid(child, child); close_fd(out_pipe[1]); close_fd(err_pipe[1]);
    if (!non_blocking(out_pipe[0]) || !non_blocking(err_pipe[0])) {
        result.launch_error = "Could not configure non-blocking pipes.";
        (void)::kill(-child, SIGKILL); int status = 0; (void)::waitpid(child, &status, 0);
        close_fd(out_pipe[0]); close_fd(err_pipe[0]); return result;
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool exited = false, out_open = true, err_open = true; int status = 0; char buffer[8192];
    while (out_open || err_open || !exited) {
        if (!exited && ::waitpid(child, &status, WNOHANG) == child) exited = true;
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline && (!exited || out_open || err_open)) {
            result.timed_out = true; (void)::kill(-child, SIGKILL);
            if (!exited) { (void)::waitpid(child, &status, 0); exited = true; }
        }
        pollfd fds[2]{{out_pipe[0], static_cast<short>(POLLIN | POLLHUP), 0},
                      {err_pipe[0], static_cast<short>(POLLIN | POLLHUP), 0}};
        const int wait_ms = exited ? 20 : static_cast<int>(std::clamp<long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count(), 0, 50));
        (void)::poll(fds, 2, wait_ms);
        const auto drain = [&](int& fd, bool& open, std::string& output) {
            if (!open) return;
            while (true) {
                const auto n = ::read(fd, buffer, sizeof(buffer));
                if (n > 0) {
                    append_bounded(output, buffer, static_cast<std::size_t>(n), max_output_bytes_, result.output_limit_exceeded);
                    if (result.output_limit_exceeded) {
                        (void)::kill(-child, SIGKILL);
                        if (!exited) { (void)::waitpid(child, &status, 0); exited = true; }
                        break;
                    }
                } else if (n == 0) { close_fd(fd); open = false; break; }
                else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
                else { close_fd(fd); open = false; break; }
            }
        };
        drain(out_pipe[0], out_open, result.standard_output);
        drain(err_pipe[0], err_open, result.standard_error);
        if (exited && !out_open && !err_open) break;
    }
    close_fd(out_pipe[0]); close_fd(err_pipe[0]);
    if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) result.exit_code = 128 + WTERMSIG(status);
#endif
    return result;
}

} // namespace telegram_media
