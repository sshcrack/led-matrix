#include <algorithm>
#include <cmath>

#include "HipHopScenes.h"

namespace Scenes {
using namespace AudioKit;

namespace {
// Design space 128x128, seen from straight above.
constexpr float PlatterX = 45.0f, PlatterY = 50.0f;
constexpr float PlatterR = 39.0f, RecordR = 36.5f, LabelR = 11.0f;
constexpr float PivotX = 81.0f, PivotY = 12.0f;
constexpr float MixerX0 = 91.0f, MixerX1 = 125.0f, DeckY0 = 4.0f, DeckY1 = 97.0f;
constexpr float ChannelX[] = {99.0f, 117.0f};
constexpr float Rpm = 33.333f;
constexpr float ScratchRadius = 29.0f;  // where the hand rests on the record
constexpr float LightAngle = -2.3f;     // the room light comes from the upper left

// Anti-aliased opaque capsule.
void capsule(Buffer& b, float sx, float sy, float ax, float ay, float bx, float by, float r, const RGB& c, float alpha = 1.0f)
{
    const float vx = bx - ax, vy = by - ay;
    const float l2 = std::max(1e-4f, vx * vx + vy * vy);
    const int x0 = static_cast<int>((std::min(ax, bx) - r - 1.0f) * sx), x1 = static_cast<int>((std::max(ax, bx) + r + 1.0f) * sx);
    const int y0 = static_cast<int>((std::min(ay, by) - r - 1.0f) * sy), y1 = static_cast<int>((std::max(ay, by) + r + 1.0f) * sy);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const float px = (x + 0.5f) / sx, py = (y + 0.5f) / sy;
            const float t = std::clamp(((px - ax) * vx + (py - ay) * vy) / l2, 0.0f, 1.0f);
            const float d = len(px - ax - vx * t, py - ay - vy * t);
            const float a = std::clamp(r + 0.5f - d, 0.0f, 1.0f) * alpha;
            if (a > 0.0f)
                b.mix(x, y, c, a);
        }
}
}  // namespace

void AudioTurntableScene::register_properties()
{
    add_property(sensitivity_);
    add_property(scratching_);
}

void AudioTurntableScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    const size_t n = static_cast<size_t>(W * H);
    booth_.assign(n, {});
    radius_.assign(n, 1e3f);
    angle_.assign(n, 0.0f);
    sheen_.assign(n, 0.0f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y * W + x);
            const float gx = (x + 0.5f) / sx, gy = (y + 0.5f) / sy;
            radius_[i] = len(gx - PlatterX, gy - PlatterY);
            angle_[i] = std::atan2(gy - PlatterY, gx - PlatterX);
            // The sheen on the vinyl stays put while the record turns, so it is static.
            sheen_[i] = std::pow(std::fabs(std::cos(angle_[i] - LightAngle)), 18.0f);
            // Booth top: dark laminate with a faint grain.
            const float grain = 0.9f + 0.2f * hash(static_cast<uint32_t>(y * 31 + (x / 3) * 7));
            RGB c = RGB{20, 18, 24} * grain;
            const bool deck = gx >= 3.0f && gx < 88.0f && gy >= DeckY0 && gy < DeckY1;
            const bool mixer = gx >= MixerX0 && gx < MixerX1 && gy >= DeckY0 && gy < DeckY1;
            if (deck || mixer) {
                c = deck ? RGB{46, 44, 50} : RGB{38, 38, 46};
                const float ex = deck ? std::min(gx - 3.0f, 88.0f - gx) : std::min(gx - MixerX0, MixerX1 - gx);
                const float ey = std::min(gy - DeckY0, DeckY1 - gy);
                if (std::min(ex, ey) < 1.0f)
                    c = (gx - (deck ? 3.0f : MixerX0) < 1.0f || gy - DeckY0 < 1.0f) ? RGB{90, 88, 96} : RGB{14, 14, 18};
                // Brushed finish on the deck.
                if (deck)
                    c = c * (0.94f + 0.08f * std::sin(gy * 3.1f));
            }
            // Soft shadow of the gear on the booth.
            if (!deck && !mixer &&
                ((gx >= 88.0f && gx < 90.0f && gy > DeckY0 + 2.0f && gy < DeckY1 + 2.0f) ||
                 (gy >= DeckY1 && gy < DeckY1 + 2.0f && gx > 5.0f && gx < MixerX1 + 2.0f)))
                c = c * 0.5f;
            booth_[i] = c;
        }
    auto R = [&](float x0, float y0, float x1, float y1, const RGB& c) {
        for (int y = static_cast<int>(y0 * sy); y < static_cast<int>(y1 * sy); ++y)
            for (int x = static_cast<int>(x0 * sx); x < static_cast<int>(x1 * sx); ++x)
                if (x >= 0 && y >= 0 && x < W && y < H)
                    booth_[static_cast<size_t>(y * W + x)] = c;
    };
    // Pitch fader slot, start button, the target light.
    R(83.0f, 58.0f, 85.0f, 92.0f, RGB{10, 10, 12});
    R(6.0f, 87.0f, 15.0f, 94.0f, RGB{70, 70, 78});
    R(7.0f, 88.0f, 14.0f, 93.0f, RGB{110, 110, 118});
    // Mixer: fader slots, crossfader slot.
    for (float cx : ChannelX) R(cx - 0.5f, 58.0f, cx + 1.0f, 86.0f, RGB{8, 8, 10});
    R(94.0f, 90.0f, 123.0f, 92.5f, RGB{8, 8, 10});
    R(94.0f, 92.5f, 123.0f, 93.0f, RGB{70, 70, 80});  // lit lower lip of the slot
    // Headphones lying on the booth: a band and two cups.
    for (float a = 0.0f; a <= Pi; a += 0.05f) {
        const float hx = 34.0f + std::cos(a) * 15.0f, hy = 116.0f - std::sin(a) * 11.0f;
        R(hx - 1.0f, hy - 1.0f, hx + 1.0f, hy + 1.0f, RGB{40, 40, 46});
    }
    for (float cx : {19.0f, 49.0f})
        for (int y = static_cast<int>(110.0f * sy); y < static_cast<int>(124.0f * sy); ++y)
            for (int x = static_cast<int>((cx - 5.0f) * sx); x < static_cast<int>((cx + 5.0f) * sx); ++x) {
                const float d = len(((x + 0.5f) / sx - cx) / 4.5f, ((y + 0.5f) / sy - 117.0f) / 6.0f);
                if (d < 1.0f && x >= 0 && y >= 0 && x < W && y < H)
                    booth_[static_cast<size_t>(y * W + x)] = d > 0.75f ? RGB{70, 70, 78} : RGB{26, 26, 30};
            }
    // Cable from the mixer to the bottom edge.
    for (float t = 0.0f; t < 1.0f; t += 0.01f) {
        const float cx = 108.0f + std::sin(t * 4.0f) * 6.0f, cy = DeckY1 + t * 32.0f;
        R(cx, cy, cx + 1.0f, cy + 1.0f, RGB{12, 12, 14});
    }
    vu_.assign(2, 0.0f);
    vuPeak_.assign(2, 0.0f);
}

void AudioTurntableScene::draw_hand(float x, float y, float angle, float lift, float scale)
{
    const float sx = matrix_width / 128.0f, sy = matrix_height / 128.0f;
    const float ux = std::cos(angle), uy = std::sin(angle), nx = -uy, ny = ux;
    const float s = scale;
    // Shadow first, offset by how high the hand floats.
    const float off = 1.0f + lift * 2.5f;
    for (int pass = 0; pass < 2; ++pass) {
        const float ox = pass == 0 ? off : 0.0f, oy = pass == 0 ? off : 0.0f;
        const float a = pass == 0 ? 0.45f : 1.0f;
        const RGB skin = pass == 0 ? RGB{0, 0, 0} : RGB{176, 120, 88};
        const RGB sleeve = pass == 0 ? RGB{0, 0, 0} : RGB{34, 32, 44};
        // Forearm in a sleeve, out of the frame.
        capsule(buf_, sx, sy, x - ux * 8.0f * s + ox, y - uy * 8.0f * s + oy, x - ux * 60.0f * s + ox, y - uy * 60.0f * s + oy, 4.4f * s,
                sleeve, a);
        capsule(buf_, sx, sy, x - ux * 2.0f * s + ox, y - uy * 2.0f * s + oy, x - ux * 8.5f * s + ox, y - uy * 8.5f * s + oy, 3.0f * s,
                skin * 0.9f, a);
        // Palm.
        capsule(buf_, sx, sy, x - ux * 1.0f * s + ox, y - uy * 1.0f * s + oy, x + ux * 2.0f * s + ox, y + uy * 2.0f * s + oy, 3.6f * s,
                skin, a);
        // Fingers, slightly spread and curled.
        for (int f = 0; f < 4; ++f) {
            const float across = (-2.6f + f * 1.75f) * s;
            const float length = (f == 0 || f == 3 ? 4.0f : 5.0f) * s;
            const float bx = x + ux * 4.2f * s + nx * across, by = y + uy * 4.2f * s + ny * across;
            capsule(buf_, sx, sy, bx + ox, by + oy, bx + ux * length + nx * across * 0.15f + ox,
                    by + uy * length + ny * across * 0.15f + oy, 0.85f * s, pass == 0 ? skin : skin * (0.92f + 0.03f * f), a);
        }
        // Thumb on the inner side.
        const float tx = x + nx * 3.2f * s, ty = y + ny * 3.2f * s;
        capsule(buf_, sx, sy, tx + ox, ty + oy, tx + (ux * 3.0f + nx * 2.2f) * s + ox, ty + (uy * 3.0f + ny * 2.2f) * s + oy, 1.0f * s,
                skin * 0.95f, a);
    }
}

bool AudioTurntableScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    if (static_cast<int>(booth_.size()) != W * H)
        init_world();
    spectrum_.update(audio, dt, 0.03f, 0.2f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float snare = feature(audio, AudioProtocol::Feature::Snare);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat);
    const float onset = feature(audio, AudioProtocol::Feature::OnsetStrength);
    if (events_.beatHit)
        beat_.hit(0.6f + 0.4f * kick);
    if (events_.onsetHit && kick > 0.35f)
        kick_.hit(kick);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    // A scratch on strong onsets: push the record forward and drag it back.
    if (scratching_->get() && events_.onsetHit && scratch_ >= 1.0f && (onset > 0.45f || snare > 0.5f)) {
        scratch_ = 0.0f;
        scratchAmp_ = 12.0f + 8.0f * std::max(onset, snare);
    }
    // Crossfader cuts on snares and hats while scratching, otherwise it rests open.
    if (events_.onsetHit && (snare > 0.4f || hihat > 0.55f)) {
        ++cut_;
        faderTarget_ = cut_ % 2 ? 0.04f : 0.96f;
    }
    else if (scratch_ >= 1.0f && beat_.v < 0.1f) {
        faderTarget_ = 0.5f;
    }
    beat_.decay(dt, 0.2f);
    kick_.decay(dt, 0.12f);
    drop_.decay(dt, 1.8f);
    palette_.update(dt, 1.5f);
    fader_ += (faderTarget_ - fader_) * (1.0f - std::exp(-dt / 0.015f));  // a sharp snap
    sweep_ += dt * (0.25f + 0.2f * spectrum_.level);

    // Record motion: 33 1/3 rpm, except while the hand holds and drags it.
    const float spin = Rpm / 60.0f * Tau;
    if (scratch_ < 1.0f) {
        scratch_ = std::min(1.0f, scratch_ + dt / 0.42f);
        const float prev = handOffset_;
        // Forward, back, forward: a quick "baby scratch".
        handOffset_ = -scratchAmp_ * std::sin(scratch_ * Tau * 1.5f) * (1.0f - scratch_ * 0.3f);  // pull back first
        recordAngle_ += (handOffset_ - prev) / ScratchRadius;
        press_.hit();
    }
    else {
        handOffset_ *= std::exp(-dt / 0.08f);
        recordAngle_ += spin * dt;
    }
    press_.decay(dt, 0.15f);

    buf_.px = booth_;

    // --- platter rim with strobe dots, the record ------------------------------------------------------
    const RGB labelA = hsv(palette_.value, 0.75f, 0.95f), labelB = hsv(palette_.value + 150.0f, 0.6f, 0.55f);
    for (int y = static_cast<int>((PlatterY - PlatterR - 1) * sy); y <= static_cast<int>((PlatterY + PlatterR + 1) * sy); ++y)
        for (int x = static_cast<int>((PlatterX - PlatterR - 1) * sx); x <= static_cast<int>((PlatterX + PlatterR + 1) * sx); ++x) {
            if (x < 0 || y < 0 || x >= W || y >= H)
                continue;
            const size_t i = static_cast<size_t>(y * W + x);
            const float r = radius_[i] / std::min(sx, sy);
            if (r > PlatterR)
                continue;
            const float a = angle_[i];
            RGB c;
            if (r > RecordR) {
                // Platter edge: rows of strobe dots that turn with the platter.
                const float rel = a - recordAngle_;
                const float dots = 0.5f + 0.5f * std::cos(rel * 36.0f);
                c = lerp(RGB{70, 70, 78}, RGB{200, 200, 210}, dots > 0.8f && r > RecordR + 1.2f ? 1.0f : 0.25f);
            }
            else if (r > LabelR) {
                // Vinyl: fine grooves, darker track gaps, and the sheen that stays put
                // while the record turns under it.
                const float groove = 0.5f + 0.5f * std::sin(r * 1.6f);
                const bool gap = std::fabs(r - 30.5f) < 0.5f || std::fabs(r - 23.0f) < 0.5f || std::fabs(r - 16.5f) < 0.5f;
                c = RGB{16, 16, 20} + RGB{6, 6, 8} * groove;
                if (gap)
                    c = RGB{8, 8, 10};
                const float sheen = sheen_[i];
                c = c + RGB{120, 120, 140} * (sheen * (0.35f + 0.25f * groove));
                // A tint of the club light on the vinyl.
                c = c + labelA * (0.04f + 0.05f * beat_.v);
            }
            else if (r > 1.2f) {
                // Label: two-tone halves and a cue sticker, turning with the record.
                const float rel = a - recordAngle_;
                c = std::cos(rel) > 0.0f ? labelA : labelB;
                if (r > LabelR - 1.2f)
                    c = c * 0.7f;
            }
            else {
                c = RGB{210, 210, 220};  // spindle
            }
            buf_.at(x, y) = c;
        }

    // Chunky cue sticker on the vinyl next to the label: it shows the record
    // turning, stopping and being dragged back.
    {
        const float x = PlatterX + std::cos(recordAngle_) * 15.0f, y = PlatterY + std::sin(recordAngle_) * 15.0f;
        buf_.disc(x * sx, y * sy, 2.8f * sx, RGB{40, 30, 10}, false);
        buf_.disc(x * sx, y * sy, 2.2f * sx, RGB{255, 214, 50}, false);
    }

    // --- tonearm ---------------------------------------------------------------------------------------
    {
        const float needleR = 27.0f + 3.0f * std::sin(time_ * 0.01f);
        const float na = 0.72f;
        const float hx = PlatterX + std::cos(na) * needleR, hy = PlatterY + std::sin(na) * needleR;
        buf_.disc(PivotX * sx, PivotY * sy, 4.5f * sx, RGB{60, 60, 68}, false);
        buf_.disc(PivotX * sx, PivotY * sy, 2.5f * sx, RGB{150, 150, 160}, false);
        capsule(buf_, sx, sy, PivotX + 1.0f, PivotY - 3.0f, PivotX + 2.5f, PivotY - 8.0f, 2.2f, RGB{120, 120, 130});  // counterweight
        capsule(buf_, sx, sy, PivotX + 1.5f, PivotY + 1.5f, hx + 1.5f, hy + 1.5f, 0.9f, RGB{0, 0, 0}, 0.4f);          // shadow
        capsule(buf_, sx, sy, PivotX, PivotY, hx, hy - 3.0f, 0.8f, RGB{200, 200, 212});
        capsule(buf_, sx, sy, hx, hy - 3.0f, hx - 1.5f, hy + 1.0f, 1.6f, RGB{40, 40, 46});  // headshell
        // Lift lever and the pitch fader cap.
        capsule(buf_, sx, sy, 77.0f, 30.0f, 80.0f, 30.0f, 1.0f, RGB{150, 150, 160});
        const float pitch = 75.0f + 2.0f * std::sin(time_ * 0.07f);
        capsule(buf_, sx, sy, 82.0f, pitch, 86.0f, pitch, 1.3f, RGB{170, 170, 180});
    }
    // The start button glows when the deck runs.
    buf_.disc(10.5f * sx, 90.5f * sy, 1.0f * sx, RGB{255, 80, 60} * (0.6f + 0.4f * beat_.v), false);

    // --- mixer: EQ knobs, VU meters, channel faders, crossfader ----------------------------------------
    {
        const float lvl[2] = {spectrum_.band(0.0f, 0.5f), spectrum_.band(0.3f, 1.0f)};
        for (int ch = 0; ch < 2; ++ch) {
            auto& v = vu_[static_cast<size_t>(ch)];
            v = std::max(std::min(1.0f, lvl[ch] * (1.0f + 0.8f * (ch == 0 ? 1.0f - fader_ : fader_)) + 0.25f * beat_.v), v - dt * 1.2f);
            const float cx = ChannelX[ch];
            for (int k = 0; k < 3; ++k) {
                const float ky = 12.0f + k * 9.0f;
                buf_.disc(cx * sx, ky * sy, 3.2f * sx, RGB{20, 20, 24}, false);
                buf_.disc(cx * sx, ky * sy, 2.4f * sx, RGB{80, 80, 90}, false);
                const float band = spectrum_.band(k / 3.0f, (k + 1) / 3.0f);
                const float ka = -Pi * 0.5f + (band - 0.5f) * 1.6f + (k - 1) * 0.3f;
                capsule(buf_, sx, sy, cx, ky, cx + std::cos(ka) * 2.2f, ky + std::sin(ka) * 2.2f, 0.4f, RGB{240, 240, 240});
            }
            // Channel fader cap rides with the channel's level.
            const float capY = 84.0f - 22.0f * (0.6f + 0.3f * lvl[ch]);
            capsule(buf_, sx, sy, cx - 2.0f, capY, cx + 2.5f, capY, 1.3f, RGB{190, 190, 200});
            // VU: a column of LEDs between the knobs and the fader.
            auto& peak = vuPeak_[static_cast<size_t>(ch)];
            peak = std::max(v, peak - dt * 0.35f);
            const int peakSeg = std::min(11, static_cast<int>(peak * 12.0f));
            const float vx = 104.5f + ch * 5.0f;
            for (int s = 0; s < 12; ++s) {
                const float sy0 = 54.0f - s * 3.5f;
                const RGB on = s < 8 ? RGB{60, 230, 90} : (s < 10 ? RGB{240, 210, 40} : RGB{255, 50, 40});
                const bool lit = v * 12.0f > s;
                RGB c = lit ? on : on * 0.12f;
                if (s == peakSeg && peak > 0.1f)
                    c = RGB{255, 170, 40};  // amber peak hold
                capsule(buf_, sx, sy, vx, sy0, vx + 2.5f, sy0, 1.0f, c);
            }
        }
    }

    // --- hands ------------------------------------------------------------------------------------------
    {
        // Scratch hand on the left edge of the record; it presses while it drags.
        const float base = Pi * 0.93f;  // just below the left side
        const float along = handOffset_ / ScratchRadius;
        const float hx = PlatterX + std::cos(base + along) * ScratchRadius, hy = PlatterY + std::sin(base + along) * ScratchRadius;
        const float lift = scratching_->get() ? (1.0f - press_.v) : 1.5f;
        draw_hand(hx - 2.0f, hy, 0.15f + along * 0.6f, lift, 1.1f);
        // Fader hand from the lower right on the crossfader.
        const float fx = 96.5f + 24.0f * fader_;
        draw_hand(fx + 1.0f, 104.0f, -Pi * 0.5f - (fader_ - 0.5f) * 0.7f, 0.3f, 1.0f);  // from below, the wrist turns with the cut
        // Crossfader cap with a bright edge, pinched by the fingertips below it.
        capsule(buf_, sx, sy, fx, 88.0f, fx, 94.5f, 3.2f, RGB{255, 255, 255});
        capsule(buf_, sx, sy, fx, 88.5f, fx, 94.0f, 2.3f, hsv(palette_.value, 0.5f, 0.9f));
    }

    // Shift the booth down a little and run an LED strip along the far edge
    // of the booth (the crowd side); it shows the spectrum.
    {
        const int shift = static_cast<int>(6.0f * sy);
        for (int y = H - 1; y >= shift; --y)
            for (int x = 0; x < W; ++x) buf_.at(x, y) = buf_.at(x, y - shift);
        for (int y = 0; y < shift; ++y)
            for (int x = 0; x < W; ++x) buf_.at(x, y) = RGB{20, 18, 24};
        constexpr int Segments = 32;
        for (int s = 0; s < Segments; ++s) {
            const float lv = spectrum_.band(s / static_cast<float>(Segments), (s + 1) / static_cast<float>(Segments));
            const RGB col = hsv(palette_.value + s * 6.0f, 0.8f, 1.0f) * std::min(1.0f, 0.15f + 0.9f * lv + 0.2f * beat_.v);
            const int x0 = s * W / Segments, x1 = (s + 1) * W / Segments - 1;
            for (int x = x0; x < x1; ++x)
                for (int y = static_cast<int>(1.0f * sy); y < static_cast<int>(3.0f * sy); ++y) buf_.at(x, y) = col;
            glow(buf_, (x0 + x1) * 0.5f, 3.5f * sy, 3.0f * sx, col * 0.3f);
        }
    }

    // --- club light: a colour wash that sweeps over the booth, brighter on beats ------------------------
    {
        const float cx = 64.0f + 70.0f * std::sin(sweep_ * Tau * 0.5f), cy = 40.0f + 30.0f * std::cos(sweep_ * Tau * 0.35f);
        const RGB tint = hsv(palette_.value + 40.0f, 0.8f, 1.0f);
        const float amount = 0.14f + 0.14f * beat_.v + 0.15f * drop_.v;
        buf_.add_field(0, H, field_step(), [&](float x, float y) {
            const float d = len(x / sx - cx, y / sy - cy) / 48.0f;
            return tint * (amount * std::exp(-d * d * 2.0f));
        });
    }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioTurntableScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "turntable";
    d.tags = {"music",       "dj",           "club",           "night",        "audio-reactive",
              "beat-driven", "onset-driven", "music-showcase", "style:hiphop", "style:party"};
    d.intensity = 0.45f;
    d.motion = 0.45f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.35f;
    return d;
}
}  // namespace Scenes
