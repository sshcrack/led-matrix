#include <atomic>
#include "shared/matrix/utils/shared.h"
#include <spdlog/spdlog.h>

std::atomic interrupt_received = false;

void InterruptHandler(int signo)
{
    spdlog::info("Interrupt signal received: {}. Exiting...", signo);
    interrupt_received = true;
    exit_canvas_update = true;
}
