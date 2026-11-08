#include <cstdio>

#include "../matrix/scenes/HorseDraw.h"

// Exercise the actual rasteriser: a wrapped scene phase must not teleport
// limbs. In particular, applying a fractional frequency inside horse() breaks
// this even though every individual frame still looks plausible.
int main()
{
    using namespace HorseScenes;
    // A planted hoof must cancel forward travel exactly, including the last
    // step before lift-off. Swing feet must stay above the ground and reach
    // beyond stance endpoints, otherwise the gallop degenerates into paddling.
    for (bool front : {false, true}) {
        float reach = 0, trail = 0;
        for (int i = 0; i < 1000; ++i) {
            const float phase = i / 1000.0f;
            const Point foot = gallop_hoof(front, phase);
            if (foot.y > -0.649f)
                return 1;
            reach = std::max(reach, foot.x);
            trail = std::min(trail, foot.x);
            if (phase < gallopStance - 0.002f) {
                const Point next = gallop_hoof(front, phase + 0.001f);
                if (std::abs(next.x - foot.x + gallopStride * 0.001f) > 0.0001f || std::abs(foot.y + 0.65f) > 0.0001f)
                    return 1;
            }
        }
        if (front ? reach < 10 : trail > -10)
            return 1;
        for (float boundary : {0.0f, gallopStance}) {
            const Point a = gallop_hoof(front, boundary - 0.00001f);
            const Point b = gallop_hoof(front, boundary + 0.00001f);
            if (std::abs(a.x - b.x) + std::abs(a.y - b.y) > 0.002f)
                return 1;
        }
    }
    for (float phase : {0.78f, 0.85f, 0.92f}) {
        for (int i = 0; i < 4; ++i)
            if (gallop_hoof(i % 2 == 1, phase - gallopOffsets[i]).y >= -0.8f)
                return 1;
    }
    // A walk always has support; planted feet cancel scene translation and
    // the low return arc clears the ground without becoming a high trot.
    for (int i = 0; i < 1000; ++i) {
        const float phase = i / 1000.0f;
        int contacts = 0;
        for (float offset : walkOffsets) {
            const Point foot = walk_hoof(phase - offset);
            if (foot.y > -0.651f)
                ++contacts;
            if (foot.y > -0.649f || foot.y < -2.451f)
                return 1;
        }
        if (contacts < 2)
            return 1;
        if (phase < walkStance - 0.002f) {
            const Point a = walk_hoof(phase), b = walk_hoof(phase + 0.001f);
            if (std::abs(b.x - a.x + walkStride * 0.001f) > 0.0001f)
                return 1;
        }
    }
    for (float boundary : {0.0f, walkStance}) {
        const Point a = walk_hoof(boundary - 0.00001f), b = walk_hoof(boundary + 0.00001f);
        if (std::abs(a.x - b.x) + std::abs(a.y - b.y) > 0.001f)
            return 1;
    }
    for (Gait gait : {Gait::Walk, Gait::Gallop, Gait::Graze, Gait::Fly}) {
        for (float facing : {-1.0f, 1.0f}) {
            Canvas before, after;
            before.resize(192, 128);
            after.resize(192, 128);
            horse(before, 96, 110, 3, facing, 0.99999f, gait, {1, 1, 1}, {1, 1, 1});
            horse(after, 96, 110, 3, facing, 0, gait, {1, 1, 1}, {1, 1, 1});
            float difference = 0;
            for (size_t i = 0; i < before.px.size(); ++i) {
                if (!std::isfinite(before.px[i]) || !std::isfinite(after.px[i]))
                    return 1;
                difference += std::abs(before.px[i] - after.px[i]);
            }
            std::printf("gait %d, facing %.0f: loop difference %.4f\n", static_cast<int>(gait), facing, difference);
            if (difference >= 1.0f)
                return 1;
        }
    }
}
