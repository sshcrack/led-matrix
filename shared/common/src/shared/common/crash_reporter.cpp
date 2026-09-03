#include "shared/common/crash_reporter.h"

#ifndef LED_MATRIX_CRASH_REPORTER_NO_SPDLOG
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <memory>
#include <mutex>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#else
#include <execinfo.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#include <ucontext.h>
#endif

#include <cerrno>
#include <csignal>
#endif

#ifndef PROJECT_VERSION_STRING
#define PROJECT_VERSION_STRING "unknown"
#endif
#ifndef LED_MATRIX_GIT_REVISION
#define LED_MATRIX_GIT_REVISION "unknown"
#endif

namespace CrashReporter {
namespace {

constexpr std::size_t kPathCapacity = 1024;
constexpr std::size_t kHeaderCapacity = 2048;
constexpr std::size_t kActivityCapacity = 512;
constexpr std::size_t kBreadcrumbCapacity = 96;
constexpr std::size_t kBreadcrumbTextCapacity = 512;
constexpr std::size_t kMaxFrames = 96;

struct BreadcrumbSlot {
    std::atomic<std::uint64_t> committed_sequence{0};
    std::array<char, kBreadcrumbTextCapacity> text{};
};

std::array<BreadcrumbSlot, kBreadcrumbCapacity> g_breadcrumbs;
std::atomic<std::uint64_t> g_next_breadcrumb{0};
std::array<std::array<char, kActivityCapacity>, 2> g_activity{};
std::atomic<unsigned> g_activity_index{0};
std::atomic_flag g_crash_in_progress = ATOMIC_FLAG_INIT;
std::atomic<bool> g_installed{false};
std::array<char, kHeaderCapacity> g_header{};
std::size_t g_header_length = 0;
std::array<char, kPathCapacity> g_report_path_utf8{};
std::array<char, kPathCapacity> g_pending_path_utf8{};

#ifdef _WIN32
HANDLE g_report_file = INVALID_HANDLE_VALUE;
HANDLE g_dump_file = INVALID_HANDLE_VALUE;
std::wstring g_report_path_w;
std::wstring g_pending_path_w;
std::wstring g_dump_path_w;
std::wstring g_pending_dump_path_w;
#else
int g_report_fd = -1;
struct sigaction g_previous_handlers[5]{};
constexpr std::array<int, 5> kFatalSignals{SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS};
std::array<std::byte, 64 * 1024> g_alt_signal_stack{};
#endif

std::size_t bounded_strlen(const char* text, std::size_t capacity) noexcept
{
    std::size_t size = 0;
    while (size < capacity && text[size] != '\0') ++size;
    return size;
}

void record_breadcrumb(std::string_view text) noexcept
{
    const auto sequence = g_next_breadcrumb.fetch_add(1, std::memory_order_relaxed);
    auto& slot = g_breadcrumbs[sequence % g_breadcrumbs.size()];
    slot.committed_sequence.store(0, std::memory_order_release);
    const auto count = std::min(text.size(), slot.text.size() - 1);
    std::memcpy(slot.text.data(), text.data(), count);
    slot.text[count] = '\0';
    slot.committed_sequence.store(sequence + 1, std::memory_order_release);
}

#ifndef LED_MATRIX_CRASH_REPORTER_NO_SPDLOG
class BreadcrumbSink final : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        record_breadcrumb(std::string_view(formatted.data(), formatted.size()));
    }

    void flush_() override {}
};
#endif

void fill_timestamp(char* out, std::size_t capacity) noexcept
{
    std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::strftime(out, capacity, "%Y%m%dT%H%M%SZ", &utc);
}

#ifdef _WIN32
void write_bytes(HANDLE file, const char* data, std::size_t size) noexcept
{
    if (file == INVALID_HANDLE_VALUE || !data || size == 0)
        return;
    while (size > 0) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(size, 0x7fffffffU));
        DWORD written = 0;
        if (!WriteFile(file, data, chunk, &written, nullptr) || written == 0)
            return;
        data += written;
        size -= written;
    }
}

void write_cstr(HANDLE file, const char* text) noexcept
{
    write_bytes(file, text, std::strlen(text));
}
#else
void write_bytes(int fd, const char* data, std::size_t size) noexcept
{
    if (fd < 0 || !data || size == 0)
        return;
    while (size > 0) {
        const auto written = ::write(fd, data, size);
        if (written > 0) {
            data += written;
            size -= static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        return;
    }
}

void write_cstr(int fd, const char* text) noexcept
{
    write_bytes(fd, text, bounded_strlen(text, kHeaderCapacity));
}
#endif

#ifdef _WIN32
using CrashFile = HANDLE;
#else
using CrashFile = int;
#endif

void write_hex(CrashFile file, std::uintptr_t value) noexcept
{
    char buffer[2 + sizeof(value) * 2 + 2]{};
    static constexpr char digits[] = "0123456789abcdef";
    buffer[0] = '0';
    buffer[1] = 'x';
    std::size_t index = 2;
    bool started = false;
    for (int shift = static_cast<int>((sizeof(value) - 1) * 8); shift >= 0; shift -= 8) {
        const auto byte = static_cast<unsigned>((value >> shift) & 0xffU);
        const auto hi = byte >> 4U;
        const auto lo = byte & 0xfU;
        if (hi != 0 || started) {
            buffer[index++] = digits[hi];
            started = true;
        }
        if (lo != 0 || started) {
            buffer[index++] = digits[lo];
            started = true;
        }
    }
    if (!started)
        buffer[index++] = '0';
    buffer[index++] = '\n';
    write_bytes(file, buffer, index);
}

void write_unsigned(CrashFile file, std::uint64_t value) noexcept
{
    char buffer[32]{};
    std::size_t pos = sizeof(buffer);
    do {
        buffer[--pos] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value > 0 && pos > 0);
    write_bytes(file, buffer + pos, sizeof(buffer) - pos);
}

void dump_activity(CrashFile file) noexcept
{
    write_cstr(file, "activity: ");
    const auto index = g_activity_index.load(std::memory_order_acquire) & 1U;
    const auto& activity = g_activity[index];
    const auto size = bounded_strlen(activity.data(), activity.size());
    if (size == 0)
        write_cstr(file, "(none)\n");
    else {
        write_bytes(file, activity.data(), size);
        write_cstr(file, "\n");
    }
}

void dump_breadcrumbs(CrashFile file) noexcept
{
    write_cstr(file, "\nrecent_log_breadcrumbs:\n");
    const auto end = g_next_breadcrumb.load(std::memory_order_acquire);
    const auto start = end > g_breadcrumbs.size() ? end - g_breadcrumbs.size() : 0;
    for (auto sequence = start; sequence < end; ++sequence) {
        const auto& slot = g_breadcrumbs[sequence % g_breadcrumbs.size()];
        if (slot.committed_sequence.load(std::memory_order_acquire) != sequence + 1)
            continue;
        write_cstr(file, "  ");
        const auto size = bounded_strlen(slot.text.data(), slot.text.size());
        write_bytes(file, slot.text.data(), size);
        if (size == 0 || slot.text[size - 1] != '\n')
            write_cstr(file, "\n");
    }
}

#ifdef _WIN32
void dump_windows_stack(HANDLE file, CONTEXT* context) noexcept
{
    write_cstr(file, "\nstack_trace:\n");
    CONTEXT local_context{};
    if (!context) {
        RtlCaptureContext(&local_context);
        context = &local_context;
    }

    STACKFRAME64 frame{};
    DWORD machine = 0;
#if defined(_M_X64)
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context->Rip;
    frame.AddrFrame.Offset = context->Rbp;
    frame.AddrStack.Offset = context->Rsp;
#elif defined(_M_IX86)
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context->Eip;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrStack.Offset = context->Esp;
#elif defined(_M_ARM64)
    machine = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset = context->Pc;
    frame.AddrFrame.Offset = context->Fp;
    frame.AddrStack.Offset = context->Sp;
#else
    write_cstr(file, "  stack walking is unsupported on this Windows architecture\n");
    return;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    const DWORD64 seed_address = frame.AddrPC.Offset;
    std::size_t printed_frames = 0;
    for (std::size_t walk_index = 0; walk_index < kMaxFrames && printed_frames < kMaxFrames; ++walk_index) {
        if (walk_index != 0 &&
            !StackWalk64(machine, process, thread, &frame, context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        const DWORD64 address = frame.AddrPC.Offset;
        if (address == 0)
            break;
        // On x64 DbgHelp may return the seed PC again on the first call to
        // StackWalk64. Do not duplicate the crashing frame in the report.
        if (walk_index == 1 && address == seed_address)
            continue;

        char line[2048]{};
        char symbol_storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        const bool has_symbol = SymFromAddr(process, address, &displacement, symbol) == TRUE;

        IMAGEHLP_MODULE64 module{};
        module.SizeOfStruct = sizeof(module);
        const bool has_module = SymGetModuleInfo64(process, address, &module) == TRUE;

        IMAGEHLP_LINE64 source{};
        source.SizeOfStruct = sizeof(source);
        DWORD line_displacement = 0;
        const bool has_line = SymGetLineFromAddr64(process, address, &line_displacement, &source) == TRUE;

        int count = std::snprintf(line, sizeof(line), "  #%zu 0x%llx %s%s%s%s%s", printed_frames,
                                  static_cast<unsigned long long>(address),
                                  has_module ? module.ModuleName : "", has_module ? "!" : "", has_symbol ? symbol->Name : "<unknown>",
                                  has_symbol && displacement ? "+0x" : "", "");
        if (count > 0)
            write_bytes(file, line, static_cast<std::size_t>(std::min<int>(count, sizeof(line) - 1)));
        if (has_symbol && displacement) {
            char disp[32]{};
            const int disp_count = std::snprintf(disp, sizeof(disp), "%llx", static_cast<unsigned long long>(displacement));
            if (disp_count > 0)
                write_bytes(file, disp, static_cast<std::size_t>(disp_count));
        }
        if (has_line && source.FileName) {
            char source_line[1024]{};
            const int source_count = std::snprintf(source_line, sizeof(source_line), " (%s:%lu)", source.FileName, source.LineNumber);
            if (source_count > 0)
                write_bytes(file, source_line, static_cast<std::size_t>(std::min<int>(source_count, sizeof(source_line) - 1)));
        }
        write_cstr(file, "\n");
        ++printed_frames;
    }
}

bool write_windows_minidump(EXCEPTION_POINTERS* exception) noexcept
{
    if (g_dump_file == INVALID_HANDLE_VALUE)
        return false;
    MINIDUMP_EXCEPTION_INFORMATION info{};
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = exception;
    info.ClientPointers = FALSE;
    const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
                                                 MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs);
    const bool wrote_dump = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), g_dump_file, type,
                                               exception ? &info : nullptr, nullptr, nullptr) == TRUE;
    FlushFileBuffers(g_dump_file);
    CloseHandle(g_dump_file);
    g_dump_file = INVALID_HANDLE_VALUE;
    if (!wrote_dump) {
        DeleteFileW(g_pending_dump_path_w.c_str());
        return false;
    }
    if (MoveFileExW(g_pending_dump_path_w.c_str(), g_dump_path_w.c_str(), MOVEFILE_REPLACE_EXISTING) == FALSE) {
        DeleteFileW(g_pending_dump_path_w.c_str());
        return false;
    }
    return true;
}

void finalize_windows_report() noexcept
{
    if (g_report_file == INVALID_HANDLE_VALUE)
        return;
    FlushFileBuffers(g_report_file);
    CloseHandle(g_report_file);
    g_report_file = INVALID_HANDLE_VALUE;
    MoveFileExW(g_pending_path_w.c_str(), g_report_path_w.c_str(), MOVEFILE_REPLACE_EXISTING);
}

void write_windows_report(const char* reason, EXCEPTION_POINTERS* exception, const char* message = nullptr) noexcept
{
    if (g_crash_in_progress.test_and_set(std::memory_order_acq_rel))
        return;
    if (g_report_file == INVALID_HANDLE_VALUE)
        return;

    write_bytes(g_report_file, g_header.data(), g_header_length);
    write_cstr(g_report_file, "reason: ");
    write_cstr(g_report_file, reason);
    write_cstr(g_report_file, "\nthread_id: ");
    write_unsigned(g_report_file, GetCurrentThreadId());
    write_cstr(g_report_file, "\n");
    if (message && *message) {
        write_cstr(g_report_file, "message: ");
        write_cstr(g_report_file, message);
        write_cstr(g_report_file, "\n");
    }
    CONTEXT* context = nullptr;
    if (exception && exception->ExceptionRecord) {
        write_cstr(g_report_file, "exception_code: ");
        write_hex(g_report_file, exception->ExceptionRecord->ExceptionCode);
        write_cstr(g_report_file, "exception_address: ");
        write_hex(g_report_file, reinterpret_cast<std::uintptr_t>(exception->ExceptionRecord->ExceptionAddress));
        context = exception->ContextRecord;
    }
    dump_activity(g_report_file);
    dump_breadcrumbs(g_report_file);
    dump_windows_stack(g_report_file, context);
    const bool minidump_created = write_windows_minidump(exception);
    write_cstr(g_report_file, "\nminidump: ");
    write_cstr(g_report_file, minidump_created ? "created alongside this report" : "unavailable");
    write_cstr(g_report_file, "\n");
    finalize_windows_report();
}

LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* exception) noexcept
{
    write_windows_report("unhandled_windows_exception", exception);
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
void dump_proc_maps(int file) noexcept
{
#if defined(__linux__)
    const int maps = ::open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (maps < 0)
        return;
    write_cstr(file, "\nprocess_memory_map:\n");
    char buffer[4096];
    while (true) {
        const auto read_count = ::read(maps, buffer, sizeof(buffer));
        if (read_count > 0) {
            write_bytes(file, buffer, static_cast<std::size_t>(read_count));
            continue;
        }
        if (read_count < 0 && errno == EINTR)
            continue;
        break;
    }
    ::close(maps);
#else
    (void)file;
#endif
}

void dump_posix_stack(int file) noexcept
{
    write_cstr(file, "\nstack_trace:\n");
    void* frames[kMaxFrames]{};
    const int count = ::backtrace(frames, static_cast<int>(kMaxFrames));
    if (count <= 0) {
        write_cstr(file, "  <backtrace unavailable>\n");
        return;
    }
    // backtrace_symbols_fd avoids the heap allocation performed by
    // backtrace_symbols(). install() primes libgcc ahead of the crash path.
    ::backtrace_symbols_fd(frames, count, file);
}

const char* signal_name(int signal) noexcept
{
    switch (signal) {
        case SIGSEGV:
            return "SIGSEGV";
        case SIGABRT:
            return "SIGABRT";
        case SIGFPE:
            return "SIGFPE";
        case SIGILL:
            return "SIGILL";
        case SIGBUS:
            return "SIGBUS";
        default:
            return "UNKNOWN";
    }
}

void finalize_posix_report() noexcept
{
    if (g_report_fd < 0)
        return;
    ::fsync(g_report_fd);
    ::close(g_report_fd);
    g_report_fd = -1;
    // The crash directory is created before any matrix privilege drop and the
    // Debian package owns it by pi:pi. rename() is async-signal-safe on POSIX.
    ::rename(g_pending_path_utf8.data(), g_report_path_utf8.data());
}

void write_posix_report(const char* reason, int signal, siginfo_t* info, void* raw_context, const char* message = nullptr) noexcept
{
    if (g_crash_in_progress.test_and_set(std::memory_order_acq_rel))
        return;
    const int file = g_report_fd;
    if (file < 0)
        return;

    write_bytes(file, g_header.data(), g_header_length);
    write_cstr(file, "reason: ");
    write_cstr(file, reason);
    write_cstr(file, "\n");
#if defined(__linux__)
    write_cstr(file, "thread_id: ");
    write_unsigned(file, static_cast<std::uint64_t>(::syscall(SYS_gettid)));
    write_cstr(file, "\n");
    if (raw_context) {
        auto* context = static_cast<ucontext_t*>(raw_context);
#if defined(__x86_64__)
        write_cstr(file, "instruction_pointer: ");
        write_hex(file, static_cast<std::uintptr_t>(context->uc_mcontext.gregs[REG_RIP]));
        write_cstr(file, "stack_pointer: ");
        write_hex(file, static_cast<std::uintptr_t>(context->uc_mcontext.gregs[REG_RSP]));
#elif defined(__aarch64__)
        write_cstr(file, "instruction_pointer: ");
        write_hex(file, static_cast<std::uintptr_t>(context->uc_mcontext.pc));
        write_cstr(file, "stack_pointer: ");
        write_hex(file, static_cast<std::uintptr_t>(context->uc_mcontext.sp));
#elif defined(__arm__)
        write_cstr(file, "instruction_pointer: ");
        write_hex(file, static_cast<std::uintptr_t>(context->uc_mcontext.arm_pc));
        write_cstr(file, "stack_pointer: ");
        write_hex(file, static_cast<std::uintptr_t>(context->uc_mcontext.arm_sp));
#endif
    }
#else
    (void)raw_context;
#endif
    if (signal != 0) {
        write_cstr(file, "signal: ");
        write_unsigned(file, static_cast<std::uint64_t>(signal));
        write_cstr(file, " (");
        write_cstr(file, signal_name(signal));
        write_cstr(file, ")\n");
    }
    if (info) {
        write_cstr(file, "fault_address: ");
        write_hex(file, reinterpret_cast<std::uintptr_t>(info->si_addr));
    }
    if (message && *message) {
        write_cstr(file, "message: ");
        write_cstr(file, message);
        write_cstr(file, "\n");
    }
    dump_activity(file);
    dump_breadcrumbs(file);
    dump_posix_stack(file);
    dump_proc_maps(file);
    finalize_posix_report();
}

void posix_signal_handler(int signal, siginfo_t* info, void* context) noexcept
{
    write_posix_report("fatal_posix_signal", signal, info, context);
    _exit(128 + signal);
}
#endif

void terminate_handler() noexcept
{
    const char* message = "std::terminate called";
    std::array<char, 768> exception_buffer{};
    try {
        if (auto current = std::current_exception()) {
            try {
                std::rethrow_exception(current);
            }
            catch (const std::exception& error) {
                std::snprintf(exception_buffer.data(), exception_buffer.size(), "uncaught C++ exception: %s", error.what());
                message = exception_buffer.data();
            }
            catch (...) {
                message = "uncaught non-std C++ exception";
            }
        }
    }
    catch (...) {
        message = "std::terminate called (failed to inspect current exception)";
    }
#ifdef _WIN32
    write_windows_report("std_terminate", nullptr, message);
    TerminateProcess(GetCurrentProcess(), 134);
#else
    write_posix_report("std_terminate", 0, nullptr, nullptr, message);
    _exit(134);
#endif
}

void cleanup_pending_report() noexcept
{
#ifdef _WIN32
    if (g_report_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_report_file);
        g_report_file = INVALID_HANDLE_VALUE;
        DeleteFileW(g_pending_path_w.c_str());
    }
    if (g_dump_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_dump_file);
        g_dump_file = INVALID_HANDLE_VALUE;
        DeleteFileW(g_pending_dump_path_w.c_str());
    }
#else
    if (g_report_fd >= 0) {
        ::close(g_report_fd);
        g_report_fd = -1;
        ::unlink(g_pending_path_utf8.data());
    }
#endif
}

}  // namespace

void install(const Config& config) noexcept
{
    if (g_installed.exchange(true, std::memory_order_acq_rel))
        return;
    try {
        std::filesystem::create_directories(config.report_directory);
        char timestamp[32]{};
        fill_timestamp(timestamp, sizeof(timestamp));
#ifdef _WIN32
        const auto pid = static_cast<unsigned long>(GetCurrentProcessId());
#else
        const auto pid = static_cast<unsigned long>(::getpid());
#endif
        const std::string base_name = "crash-" + std::string(config.process_name) + "-" + timestamp + "-pid" + std::to_string(pid);
        const auto report = config.report_directory / (base_name + ".txt");
        const auto pending = config.report_directory / ("." + base_name + ".pending");
        const auto report_string = report.string();
        const auto pending_string = pending.string();
        std::snprintf(g_report_path_utf8.data(), g_report_path_utf8.size(), "%s", report_string.c_str());
        std::snprintf(g_pending_path_utf8.data(), g_pending_path_utf8.size(), "%s", pending_string.c_str());

        g_header_length = static_cast<std::size_t>(
            std::max(0, std::snprintf(g_header.data(), g_header.size(),
                                      "LED Matrix crash report\nformat_version: 1\nprocess: %.*s\nversion: %s\ngit_revision: %s\nbuild: %s "
                                      "%s\npid: %lu\nreport_path: %s\n",
                                      static_cast<int>(config.process_name.size()), config.process_name.data(), PROJECT_VERSION_STRING,
                                      LED_MATRIX_GIT_REVISION, __DATE__, __TIME__, pid, report_string.c_str())));
        g_header_length = std::min(g_header_length, g_header.size() - 1);

#ifdef _WIN32
        g_report_path_w = report.wstring();
        g_pending_path_w = pending.wstring();
        g_dump_path_w = (config.report_directory / (base_name + ".dmp")).wstring();
        g_pending_dump_path_w = (config.report_directory / ("." + base_name + ".dmp.pending")).wstring();
        g_report_file =
            CreateFileW(g_pending_path_w.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        g_dump_file = CreateFileW(g_pending_dump_path_w.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        SetUnhandledExceptionFilter(unhandled_exception_filter);
#else
        g_report_fd = ::open(g_pending_path_utf8.data(), O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0600);
        // The matrix service opens its crash file while still privileged, then
        // drops to the owner of /var/lib/led-matrix. Match the directory owner
        // up front so the final 0600 report remains readable after the drop.
        if (g_report_fd >= 0 && ::geteuid() == 0) {
            struct stat directory_stat{};
            if (::stat(config.report_directory.c_str(), &directory_stat) == 0) {
                const int ownership_result = ::fchown(g_report_fd, directory_stat.st_uid, directory_stat.st_gid);
                (void)ownership_result;
            }
        }
        stack_t stack{};
        stack.ss_sp = g_alt_signal_stack.data();
        stack.ss_size = g_alt_signal_stack.size();
        stack.ss_flags = 0;
        ::sigaltstack(&stack, nullptr);
        for (std::size_t index = 0; index < kFatalSignals.size(); ++index) {
            struct sigaction action{};
            action.sa_sigaction = posix_signal_handler;
            sigemptyset(&action.sa_mask);
            action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
            ::sigaction(kFatalSignals[index], &action, &g_previous_handlers[index]);
        }
        // Prime libgcc's unwinder while the process is healthy. glibc documents
        // that the first backtrace() call may dynamically load libgcc.
        void* prime[1]{};
        (void)::backtrace(prime, 1);
#endif
        std::set_terminate(terminate_handler);
        std::atexit(cleanup_pending_report);
        set_activity("starting");
    }
    catch (...) {
        // Diagnostics are best-effort and must never prevent normal startup.
    }
}

void attach_to_default_logger() noexcept
{
#ifndef LED_MATRIX_CRASH_REPORTER_NO_SPDLOG
    try {
        auto logger = spdlog::default_logger();
        if (!logger)
            return;
        auto sink = std::make_shared<BreadcrumbSink>();
        sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        sink->set_level(spdlog::level::trace);
        logger->sinks().push_back(std::move(sink));
    }
    catch (...) {
    }
#endif
}

void set_activity(std::string_view activity) noexcept
{
    const auto current = g_activity_index.load(std::memory_order_relaxed) & 1U;
    const auto next = current ^ 1U;
    auto& buffer = g_activity[next];
    const auto count = std::min(activity.size(), buffer.size() - 1);
    std::memcpy(buffer.data(), activity.data(), count);
    buffer[count] = '\0';
    g_activity_index.store(next, std::memory_order_release);
}

void report_exception(std::string_view context, std::string_view message) noexcept
{
    std::array<char, 1024> combined{};
    const auto count = std::snprintf(combined.data(), combined.size(), "%.*s: %.*s", static_cast<int>(context.size()), context.data(),
                                     static_cast<int>(message.size()), message.data());
    const char* text = count > 0 ? combined.data() : "caught top-level exception";
#ifdef _WIN32
    write_windows_report("caught_top_level_exception", nullptr, text);
#else
    write_posix_report("caught_top_level_exception", 0, nullptr, nullptr, text);
#endif
}

}  // namespace CrashReporter
