#include "utils.h"
#include "shared.h"
#include <sys/time.h>
#include <thread>
#include <spdlog/spdlog.h>

using namespace spdlog;

tmillis_t GetTimeInMillis() {
    struct timeval tp{};

    gettimeofday(&tp, nullptr);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

void SleepMillis(tmillis_t milli_seconds) {
    if (milli_seconds <= 0) return;
    tmillis_t end_time = GetTimeInMillis() + milli_seconds;

    while(GetTimeInMillis() < end_time) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(skip_image || exit_canvas_update) {
            debug("Skipping...");
            skip_image.store(false);
            break;
        }
    }
}