#include <atomic>
#include "shared/matrix/interrupt.h"
#include "shared/matrix/utils/shared.h"

std::atomic<bool> interrupt_received = false;
std::atomic<int> interrupt_signal = 0;

// Runs in signal context: only lock-free atomic stores are allowed here (no logging, no allocation).
void InterruptHandler(int signo)
{
    interrupt_signal.store(signo);
    interrupt_received = true;
    PlaybackControl::interrupt();
}
