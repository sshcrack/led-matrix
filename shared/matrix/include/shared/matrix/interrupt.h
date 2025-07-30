#pragma once
#include <atomic>

extern std::atomic<bool> interrupt_received;

extern void InterruptHandler(int signo);