#include "GenreVistaScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioCassetteScene::register_properties()
{
    add_property(sensitivity_);
    add_property(spoolSpeed_);
    add_property(ledBars_);
}

bool AudioCassetteScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    spectrum_.update(audio, dt, 0.03f, 0.18f);

    const float kickF = feature(audio, AudioProtocol::Feature::Kick);
    if (events_.beatHit)
        beat_.hit(0.5f + 0.5f * kickF);
    if (events_.onsetHit && kickF > 0.35f)
        kick_.hit(kickF);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.2f);
    kick_.decay(dt, 0.12f);
    drop_.decay(dt, 1.6f);
    palette_.update(dt, 1.2f);

    // The tape slowly moves from the left reel to the right one, then flips (side B).
    tape_ += dt / 90.0f;
    if (tape_ > 1.0f)
        tape_ = 0.0f;
    const float speed = spoolSpeed_->get() * (1.0f + 0.35f * spectrum_.level + 0.5f * drop_.v);
    const float rA = 7.5f + 9.5f * (1.0f - tape_), rB = 7.5f + 9.5f * tape_;
    reelA_ += dt * speed * 3.2f * (17.0f / rA);   // a nearly empty reel turns faster
    reelB_ += dt * speed * 3.2f * (17.0f / rB);
    wobble_ += dt * 11.0f;

    const RGB accent = hsv(palette_.value, 0.75f, 1.0f);
    // --- desk lamp background ---------------------------------------------------------------------------------------------------------
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            buf_.at(x, y) = RGB{22, 15, 12} * (0.7f + 0.3f * std::sin(y * 0.8f + std::sin(x * 0.07f) * 3.0f)) * (1.0f - 0.35f * static_cast<float>(y) / H);
    glow(buf_, W * 0.5f, H * 0.3f, W * 0.9f, RGB{255, 170, 90} * (0.07f + 0.06f * beat_.v));

    // --- cassette body ------------------------------------------------------------------------------------------------------------------------
    const float shake = kick_.v * std::sin(wobble_ * 1.7f) * 0.7f;
    const int x0 = static_cast<int>(W * 0.08f + shake), x1 = static_cast<int>(W * 0.92f + shake);
    const int y0 = static_cast<int>(H * 0.12f), y1 = static_cast<int>(H * 0.66f);
    const RGB shell{30, 30, 36};
    rect(buf_, x0, y0, x1, y1, shell);
    rect(buf_, x0, y0, x1, y0 + 1, RGB{80, 80, 90});
    rect(buf_, x0, y0, x0 + 1, y1, RGB{60, 60, 68});
    // Label with three colour stripes (deliberately blank: no readable text).
    const int ly0 = y0 + 3, ly1 = y0 + static_cast<int>((y1 - y0) * 0.42f);
    rect(buf_, x0 + 4, ly0, x1 - 4, ly1, RGB{225, 215, 190});
    const int stripe = std::max(2, (ly1 - ly0) / 4);
    for (int s = 0; s < 3; ++s) rect(buf_, x0 + 4, ly0 + 2 + s * (stripe + 1), x1 - 4, ly0 + 2 + s * (stripe + 1) + stripe, hsv(palette_.value + s * 40.0f, 0.8f, 0.95f) * (0.75f + 0.25f * beat_.v));
    for (int x = x0 + 8; x < x1 - 8; x += 4) buf_.set(x, ly1 - 2, RGB{150, 140, 120});  // pen-line hint
    // Screws in the corners.
    for (int sx : {x0 + 3, x1 - 4})
        for (int sy : {y0 + 2, y1 - 3}) buf_.disc(static_cast<float>(sx) + 0.5f, static_cast<float>(sy) + 0.5f, 1.2f, RGB{110, 110, 120}, false);

    // --- window with the two reels ---------------------------------------------------------------------------------------------------------------
    const int wx0 = x0 + static_cast<int>((x1 - x0) * 0.16f), wx1 = x1 - static_cast<int>((x1 - x0) * 0.16f);
    const int wy0 = ly1 + 3, wy1 = y1 - 5;
    rect(buf_, wx0, wy0, wx1, wy1, RGB{6, 6, 9});
    const float cyR = (wy0 + wy1) * 0.5f;
    const float cxA = wx0 + (wx1 - wx0) * 0.24f, cxB = wx0 + (wx1 - wx0) * 0.76f;
    auto reel = [&](float cx, float r, float angle) {
        buf_.disc(cx, cyR, r, RGB{54, 32, 20}, false);                                     // wound tape
        buf_.disc(cx, cyR, r, RGB{150, 100, 60} * (0.10f + 0.05f * beat_.v), true);         // slight sheen
        for (int i = 0; i < 40; ++i) {                                                       // winding texture rings
            const float rr = 5.0f + i * 0.6f;
            if (rr > r)
                break;
            for (int a = 0; a < 24; ++a)
                buf_.add(static_cast<int>(cx + std::cos(a * Tau / 24.0f) * rr), static_cast<int>(cyR + std::sin(a * Tau / 24.0f) * rr), RGB{10, 6, 3});
        }
        buf_.disc(cx, cyR, 5.2f, RGB{210, 210, 215}, false);                                  // hub
        buf_.disc(cx, cyR, 3.2f, RGB{12, 12, 16}, false);
        for (int t = 0; t < 6; ++t) {                                                          // spokes / teeth
            const float a = angle + t * Tau / 6.0f;
            line(buf_, cx + std::cos(a) * 2.0f, cyR + std::sin(a) * 2.0f, cx + std::cos(a) * 4.8f, cyR + std::sin(a) * 4.8f, RGB{225, 225, 230});
        }
    };
    reel(cxA, rA, reelA_);
    reel(cxB, rB, reelB_);
    // Tape path to the bottom head, wobbling slightly with the bass.
    const float headX = (cxA + cxB) * 0.5f, headY = static_cast<float>(wy1) - 0.5f;
    const float flutter = std::sin(wobble_) * 0.35f * (0.4f + spectrum_.band(0.0f, 0.2f));
    line(buf_, cxA - rA * 0.7f, cyR + rA * 0.7f, headX - 8.0f, headY + flutter, RGB{70, 44, 28});
    line(buf_, headX - 8.0f, headY + flutter, headX + 8.0f, headY + flutter, RGB{70, 44, 28});
    line(buf_, headX + 8.0f, headY + flutter, cxB + rB * 0.7f, cyR + rB * 0.7f, RGB{70, 44, 28});
    // Read head glows while there is signal.
    rect(buf_, static_cast<int>(headX) - 3, wy1 - 1, static_cast<int>(headX) + 4, wy1 + 1, RGB{160, 160, 170});
    glow(buf_, headX, static_cast<float>(wy1), 10.0f, accent * (0.25f + 0.8f * spectrum_.level));

    // --- LED level bars below, like a hi-fi deck -----------------------------------------------------------------------------------------------------
    if (ledBars_->get()) {
        const int bars = 16;
        const int bw = (W - 12) / bars;
        const int segments = 9;
        const int by1 = H - 4, segH = std::max(2, (H - y1 - 10) / segments);
        for (int b = 0; b < bars; ++b) {
            const float v = spectrum_.value[static_cast<size_t>(b * spectrum_.value.size() / bars)];
            const float pk = spectrum_.peak[static_cast<size_t>(b * spectrum_.value.size() / bars)];
            const int lit = static_cast<int>(v * segments + 0.5f), pkSeg = static_cast<int>(pk * segments);
            for (int s = 0; s < segments; ++s) {
                const float t = static_cast<float>(s) / (segments - 1);
                const RGB on = lerp(lerp(RGB{40, 255, 90}, RGB{255, 220, 40}, smoothstep(0.45f, 0.7f, t)), RGB{255, 50, 40}, smoothstep(0.75f, 0.95f, t));
                const bool isOn = s < lit || s == pkSeg - 1;
                const int xx = 6 + b * bw, yy = by1 - (s + 1) * segH;
                rect(buf_, xx, yy, xx + bw - 1, yy + segH - 1, isOn ? on : on * 0.06f);
            }
        }
    }
    if (kick_.v > 0.02f)
        for (auto& p : buf_.px) p = p * (1.0f + kick_.v * 0.08f);
    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioCassetteScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "cassette";
    d.tags = {"music", "retro", "mixtape", "tape", "spectrum", "audio-reactive", "beat-driven", "music-showcase", "style:acoustic",
              "style:party"};
    d.intensity = 0.45f;
    d.motion = 0.45f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.40f;
    return d;
}
}  // namespace Scenes
