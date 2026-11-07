#include "shared/desktop/VideoStreamEngine.h"
#include "VideoStreamEngineInternal.h"
#include "shared/desktop/utils.h"
#include <algorithm>
#include <cstdio>
#include <fmt/format.h>
#include <thread>
#include <spdlog/spdlog.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#else
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace Shared {

// Downloads a short clip with yt-dlp and pipes ffmpeg output directly to the
// frame buffer so playback starts on the first decoded frame, without waiting
// for a complete .bin file to be written to disk first.
bool VideoStreamEngine::play_fast_chunk(int start_sec, int duration_sec) {
    if (!running_.load()) return false;

    auto mp4Path = fast_chunk_mp4_path();
    std::error_code ec;
    std::filesystem::remove(mp4Path, ec); // always re-download; it's a temp file

    // Download the short clip
    std::string dlCmd = build_ytdlp_command(mp4Path, start_sec, start_sec + duration_sec);
    spdlog::info("Fast chunk: downloading {}s clip starting at {}s", duration_sec, start_sec);
    const auto dl = run_command_capture(dlCmd, &running_);
    if (dl.exit_code != 0) {
        const auto detail = detail::command_error_summary(dl.output);
        if (detail.empty())
            spdlog::warn("Fast chunk download failed (exit {})", dl.exit_code);
        else
            spdlog::warn("Fast chunk download failed (exit {}): {}", dl.exit_code, detail);
        return false;
    }
    if (!running_.load()) return false;

    const bool played = play_decoded_pipe(build_ffmpeg_pipe_command(mp4Path));
    std::filesystem::remove(mp4Path, ec);
    return played;
}

// Shared cancellable decoder pipe for local fast-start clips and continuous
// network playback. The OS pipe provides backpressure: raw video is never
// accumulated in an unbounded queue or written to disk.
bool VideoStreamEngine::play_decoded_pipe(const std::string& ffCmd) {
    if (!running_.load()) return false;
    const size_t frameSize = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3;
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        spdlog::warn("Video decoder: CreatePipe failed");
        return false;
    }
    if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hRead); CloseHandle(hWrite);
        spdlog::warn("Video decoder: SetHandleInformation failed");
        return false;
    }
    // Redirect stderr to NUL explicitly. GetStdHandle(STD_ERROR_HANDLE) is not
    // reliable here: this is a GUI-subsystem process with no attached console,
    // so the "inherited" std handle can be NULL/invalid. Passing a bad handle
    // through STARTF_USESTDHANDLES is exactly the kind of edge case that can
    // make Windows fall back to creating a visible console for the child, so
    // we hand it a real (writable, inheritable) handle instead.
    SECURITY_ATTRIBUTES nulSa{};
    nulSa.nLength = sizeof(nulSa);
    nulSa.bInheritHandle = TRUE;
    nulSa.lpSecurityDescriptor = nullptr;
    HANDLE hNul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &nulSa,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = (hNul != INVALID_HANDLE_VALUE) ? hNul : nullptr;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    std::vector<char> cmdline(ffCmd.begin(), ffCmd.end());
    cmdline.push_back('\0');
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hRead); CloseHandle(hWrite);
        if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);
        spdlog::warn("Video decoder: CreateProcess failed");
        return false;
    }
    CloseHandle(hWrite);
    if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);
    int fd = _open_osfhandle((intptr_t)hRead, _O_RDONLY | _O_BINARY);
    if (fd == -1) {
        CloseHandle(hRead);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        spdlog::warn("Video decoder: _open_osfhandle failed");
        return false;
    }
    FILE* pipe = _fdopen(fd, "rb");
    if (!pipe) {
        _close(fd);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        spdlog::warn("Video decoder: _fdopen failed");
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ffmpeg_pid_ = static_cast<int>(pi.dwProcessId);
#else
    int fds[2];
    if (pipe(fds) != 0) return false;
    pid_t child = fork();
    if (child == -1) { close(fds[0]); close(fds[1]); return false; }
    if (child == 0) {
        // The shell, decoder and any helpers must be cancelled together. Killing
        // only the shell leaves ffmpeg holding the pipe open and fread blocked.
        setpgid(0, 0);
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        execl("/bin/sh", "sh", "-c", ffCmd.c_str(), (char*)nullptr);
        _exit(1);
    }
    setpgid(child, child);
    close(fds[1]);
    FILE* pipe = fdopen(fds[0], "r");
    if (!pipe) { close(fds[0]); kill(-child, SIGTERM); waitpid(child, nullptr, 0); return false; }
    ffmpeg_pid_ = child;
#endif
    if (!pipe) {
        spdlog::warn("Video decoder: failed to open ffmpeg pipe");
        return false;
    }

    std::vector<uint8_t> buffer(frameSize);
    int frames_played = 0;
    while (running_) {
        size_t got = fread(buffer.data(), 1, frameSize, pipe);
        if (got != frameSize) break; // ffmpeg finished or error
        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            current_frame_ = buffer;
        }
        if (frames_played == 0) {
            spdlog::info("Video decoder: received first frame");
            state_ = State::Playing;
            notify_status("playing");
        }
        std::function<void()> first_frame_cb;
        {
            std::lock_guard<std::mutex> lk(status_cb_mutex_);
            if (on_first_frame_ready && !first_frame_fired_.exchange(true))
                first_frame_cb = on_first_frame_ready;
        }
        if (first_frame_cb)
            first_frame_cb();
        ++frames_played;
        std::this_thread::sleep_for(std::chrono::duration<double>(1.0 / fps_));
    }

#ifdef _WIN32
    if (const pid_t pid = ffmpeg_pid_.load(); pid > 0) {
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProc) { TerminateProcess(hProc, 1); CloseHandle(hProc); }
        ffmpeg_pid_ = -1;
    }
    fclose(pipe);
#else
    // This thread owns reaping the child. Clearing ffmpeg_pid_ first means
    // stop() never signals a pid after it has been reaped (and possibly reused).
    if (const pid_t pid = ffmpeg_pid_.exchange(-1); pid > 0) {
        kill(-pid, SIGTERM);
        int status;
        for (int i = 0; i < 20; ++i) {
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (waitpid(pid, &status, WNOHANG) == 0) {
            kill(-pid, SIGKILL);
            waitpid(pid, &status, 0);
        }
    }
    fclose(pipe);
#endif

    return running_.load() && frames_played > 0;
}

} // namespace Shared
