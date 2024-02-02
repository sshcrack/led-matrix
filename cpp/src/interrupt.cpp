#include <atomic>
#include "shared.h"

std::atomic<bool> interrupt_received = false;

void InterruptHandler(int signo) {
    interrupt_received = true;
    exit_canvas_update = true;
}
