#include <shared/common/crash_reporter.h>
#include <spdlog/spdlog.h>

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::filesystem::path crash_dir = argv[1];
    const std::string mode = argv[2];

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
