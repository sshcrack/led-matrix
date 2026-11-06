#include <shared/common/crash_reporter.h>
#include <spdlog/spdlog.h>

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifndef _WIN32
#include <sys/resource.h>
#endif

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::filesystem::path crash_dir = argv[1];
    const std::string mode = argv[2];
#ifndef _WIN32
    // The crash reporter forwards fatal signals so the kernel writes a core
    // dump; the intentional crashes of this probe should not fill coredumpctl.
    const rlimit no_core{0, 0};
    setrlimit(RLIMIT_CORE, &no_core);
#endif

    CrashReporter::install({"crash-reporter-probe", crash_dir});
    CrashReporter::attach_to_default_logger();
    CrashReporter::set_activity("rendering scene 'crash_probe'");
    spdlog::info("breadcrumb immediately before intentional crash");

    if (mode == "terminate") {
        std::terminate();
    }
    if (mode == "exception") {
        CrashReporter::report_exception("probe caught exception", "synthetic failure");
        return 7;
    }
    if (mode == "segfault") {
#ifdef _WIN32
        *reinterpret_cast<volatile int*>(static_cast<std::uintptr_t>(0x1)) = 42;
#else
        std::raise(SIGSEGV);
#endif
        return 8;
    }
    return 3;
}
