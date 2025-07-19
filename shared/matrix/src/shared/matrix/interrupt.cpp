#include <atomic>
#include "shared/matrix/utils/shared.h"

std::atomic interrupt_received = false;

void InterruptHandler(int signo) {
    interrupt_received = true;
    exit_canvas_update = true;
}
