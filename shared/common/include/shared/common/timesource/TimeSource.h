#pragma once

#include <chrono>
#include <cstdint>

class TimeSource {
public:
    virtual ~TimeSource() = default;
    virtual int64_t now_ms() = 0;
};

class WallClock : public TimeSource {
public:
    int64_t now_ms() override {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};
