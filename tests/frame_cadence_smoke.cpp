#include <shared/common/frame_cadence.h>
#include <cstdlib>
#include <iostream>

int main() {
    for (const int fps : {1, 24, 30, 59, 60}) {
        FrameCadence cadence(12345, fps);
        for (int i = 0; i < fps * 20; ++i) {
            const auto deadline = cadence.deadline_us();
            const auto expected = 12345 + (static_cast<long long>(i) * 1000000 + fps - 1) / fps;
            if (deadline != expected) return 1;
            // Model scheduling/VSync jitter without a systematic phase drift.
            cadence.frame_started(deadline + (i % 3) * 1500);
        }
        const auto stalled = cadence.deadline_us() + 1700000;
        cadence.frame_started(stalled);
        if (cadence.deadline_us() <= stalled || cadence.deadline_us() > stalled + 1000000 / fps + 1)
            return 2;
        const auto next = cadence.deadline_us();
        cadence.frame_started(next);
        if (cadence.deadline_us() <= next) return 3;
    }
    std::cout << "fractional deadlines retain phase and skip missed slots\n";
}
