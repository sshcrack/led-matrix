#pragma once
#include <atomic>

extern std::atomic<bool> interrupt_received;
// Last signal number delivered to InterruptHandler (0 if none); log it outside signal context.
extern std::atomic<int> interrupt_signal;

extern void InterruptHandler(int signo);
