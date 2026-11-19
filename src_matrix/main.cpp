#include "daemon.h"
#include "recovery/recovery.h"

#include <filesystem>
#include <shared/common/crash_reporter.h>
#include "spdlog/spdlog.h"

int main(int argc, char *argv[])
{
#ifndef LED_MATRIX_DATA_DIR
#define LED_MATRIX_DATA_DIR "."
#endif
    CrashReporter::install({"led-matrix", std::filesystem::path(LED_MATRIX_DATA_DIR) / "crashes"});
    CrashReporter::attach_to_default_logger();
    CrashReporter::set_activity("initializing matrix daemon");
    try {
        // The launcher starts the installed package this way when the data
        // partition or every version is broken (docs/RECOVERY.md).
        if (const auto reason = Recovery::reason_from_args(argc, argv)) {
            CrashReporter::set_activity("recovery mode");
            return Recovery::run(argc, argv, *reason);
        }
        Daemon daemon(argc, argv);
        return daemon.run();
    } catch (const std::exception &e) {
        CrashReporter::report_exception("matrix daemon fatal exception", e.what());
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }
}
