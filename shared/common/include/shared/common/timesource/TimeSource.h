#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

class TimeSource {
public:
    virtual ~TimeSource() = default;
    virtual int64_t now_ms() = 0;
    virtual int64_t now_us() { return now_ms() * 1000; }
    virtual void sleep_us(int64_t duration_us) {
        std::this_thread::sleep_for(std::chrono::microseconds(duration_us));
    }
};

class WallClock : public TimeSource {
public:
    int64_t now_us() override {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    int64_t now_ms() override {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};
