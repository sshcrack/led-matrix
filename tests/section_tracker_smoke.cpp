#include "../plugins/AudioVisualizer/desktop/SectionTracker.h"

#include <iostream>

int main()
{
    const std::array<float, 7> warm{.7f, .65f, .5f, .25f, .15f, .1f, .1f};
    const std::array<float, 7> bright{.15f, .2f, .3f, .6f, .75f, .8f, .7f};
    for (const float dt : {0.01f, 0.02f, 0.05f}) {
        SectionTracker tracker;
        const auto feed = [&](const auto& bands, float seconds, bool active = true) {
            int events = 0;
            for (int i = 0; i < static_cast<int>(std::round(seconds / dt)); ++i)
                events += tracker.update(bands, dt, active, .5f) ? 1 : 0;
            return events;
        };
        if (feed(warm, 10) != 0 || feed(bright, .15f) != 0 || feed(warm, 3) != 0) {
            std::cerr << "startup or isolated transient emitted a section\n";
            return 1;
        }
        if (feed(bright, 3) != 1 || feed(bright, 15) != 0 || feed(warm, 3) != 1) {
            std::cerr << "sustained timbre change must emit exactly one section without an onset\n";
            return 2;
        }
        if (feed(warm, 1, false) != 0 || feed(bright, 8) != 0) {
            std::cerr << "pause/resume manufactured a section change\n";
            return 3;
        }
        SectionTracker drums;
        int events = 0;
        for (int i = 0; i < static_cast<int>(60 / dt); ++i) {
            const float t = i * dt;
            events += drums.update(std::fmod(t, .5f) < .1f ? bright : warm, dt, true, .5f);
        }
        if (events != 0) {
            std::cerr << "steady percussion manufactured structural changes\n";
            return 4;
        }
    }
    std::cout << "section tracking recognizes sustained changes, rejects hits, and resets on silence\n";
}
