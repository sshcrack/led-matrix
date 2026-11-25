#include <algorithm>
#include <cmath>

#include "HipHopScenes.h"

namespace Scenes {
using namespace AudioKit;

namespace {
// Design space 128x128.
constexpr float Roof = 11.0f;
constexpr float FacadeRight = 90.0f;
constexpr float Sidewalk = 82.0f;
constexpr float CurbY = 91.0f;
constexpr float WindowW = 11.0f, WindowH = 13.0f;
constexpr float WindowX[] = {6.0f, 27.0f, 48.0f, 69.0f};
constexpr float WindowY[] = {18.0f, 40.0f};
constexpr float HydrantX = 112.0f, HydrantY = 86.0f;

// String light strands: two catenaries across the street.
struct Strand {
    float x0, y0, x1, y1, sag;
};
constexpr Strand Strands[] = {{-2.0f, 30.0f, 130.0f, 25.0f, 9.0f}, {-2.0f, 50.0f, 130.0f, 45.0f, 7.0f}};

float strand_y(const Strand& s, float x)
{
    const float t = (x - s.x0) / (s.x1 - s.x0);
    return s.y0 + (s.y1 - s.y0) * t + s.sag * 4.0f * t * (1.0f - t);
}

// Which windows are lit and who leans out (column, row).
bool window_lit(int c, int r)
{
    return (c * 3 + r * 5) % 4 != 1;
}
bool window_dancer(int c, int r)
{
    return (c + r) % 2 == 0 && window_lit(c, r);
}
}  // namespace

void AudioBlockPartyScene::register_properties()
{
    add_property(sensitivity_);
    add_property(hydrant_);
}

void AudioBlockPartyScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    backdrop_.assign(static_cast<size_t>(W * H), {});
    crowd_.assign(static_cast<size_t>(W * H), 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float gx = (x + 0.5f) / sx, gy = (y + 0.5f) / sy;
            RGB c;
            const bool facade = gx < FacadeRight && gy >= Roof && gy < Sidewalk;
            if (gy < Sidewalk && !facade) {
                // Dusk sky over the cross street, a far row of houses on the right.
                const float t = gy / Sidewalk;
                c = t < 0.55f ? lerp(RGB{24, 18, 64}, RGB{110, 50, 110}, t / 0.55f)
                              : lerp(RGB{110, 50, 110}, RGB{250, 140, 80}, (t - 0.55f) / 0.45f);
                const int block = static_cast<int>(gx / 7.0f);
                const float roof = 60.0f - 12.0f * hash(static_cast<uint32_t>(block * 41 + 7));
                if (gx >= FacadeRight && gy > roof) {
                    c = RGB{40, 22, 50};
                    if (static_cast<int>(gy) % 4 == 1 && static_cast<int>(gx) % 3 == 1 &&
                        hash(static_cast<uint32_t>(static_cast<int>(gx) * 13 + static_cast<int>(gy) * 7)) > 0.55f)
                        c = RGB{220, 160, 80};
                }
            }
            else if (facade) {
                // Brownstone: brick courses, a cornice and a shadowed stoop door.
                const int row = static_cast<int>(gy) / 3;
                const bool mortar = static_cast<int>(gy) % 3 == 0 || (static_cast<int>(gx) + (row % 2) * 3) % 7 == 0;
                const float tone = 0.8f + 0.3f * hash(static_cast<uint32_t>(row * 131 + (static_cast<int>(gx) + (row % 2) * 3) / 7));
                c = mortar ? RGB{50, 28, 26} : RGB{110, 52, 38} * tone;
                // Warm light from the street and the strands falls off upwards.
                c = c * (0.45f + 0.55f * gy / Sidewalk);
                if (gy < Roof + 3.0f)
                    c = RGB{150, 120, 100} * (gy < Roof + 1.0f ? 1.0f : 0.6f);
                if (gx > 51.0f && gx < 61.0f && gy > 62.0f)
                    c = gy < 64.0f ? RGB{150, 120, 100} : RGB{40, 20, 16};  // door
                if (gx > 48.0f && gx < 64.0f && gy > 76.0f)
                    c = RGB{96, 74, 66} * (static_cast<int>(gy) % 2 == 0 ? 0.8f : 1.0f);  // stoop steps
            }
            else if (gy < CurbY) {
                c = RGB{78, 70, 74} * (static_cast<int>(gx) % 12 == 0 ? 0.7f : 1.0f);
            }
            else if (gy < CurbY + 1.5f) {
                c = RGB{110, 100, 100};
            }
            else {
                const float grain = 0.85f + 0.3f * hash(static_cast<uint32_t>(x * 7919 + y * 104729));
                c = RGB{38, 34, 44} * grain;
            }
            backdrop_[static_cast<size_t>(y * W + x)] = c;
        }
    // Fire escape over the right two columns.
    auto px = [&](float gx, float gy, const RGB& c) {
        backdrop_[static_cast<size_t>(std::clamp(static_cast<int>(gy * sy), 0, H - 1) * W +
                                      std::clamp(static_cast<int>(gx * sx), 0, W - 1))] = c;
    };
    const RGB iron{22, 16, 22};
    for (float y : {WindowY[0] + WindowH + 1.0f, WindowY[1] + WindowH + 1.0f}) {
        for (float x = 64.0f; x < FacadeRight; x += 0.5f) {
            px(x, y, iron);
            px(x, y - 4.0f, iron);
            if (static_cast<int>(x) % 3 == 0)
                for (float r = 1.0f; r < 4.0f; r += 1.0f) px(x, y - r, iron);
        }
    }
    for (float t = 0.0f; t < 1.0f; t += 0.02f) {
        px(66.0f + 20.0f * t, WindowY[0] + WindowH + 1.0f + (WindowY[1] - WindowY[0]) * t, iron);
        px(86.0f - 20.0f * t, WindowY[1] + WindowH + 1.0f + 14.0f * t, iron);
    }
    // Lamppost on the far corner.
    for (float y = 22.0f; y < CurbY; y += 0.5f) px(124.0f, y, RGB{20, 16, 24});
    people_.clear();
    // Back row smaller and further up; front row big and cut off by the frame.
    // Width varies the build; styles are fixed so every frame has a few hands up.
    for (int i = 0; i < 6; ++i)
        people_.push_back(
            {14.0f + i * 20.0f + (rnd() - 0.5f) * 6.0f, rnd() * Tau, 0.8f + rnd() * 0.1f, 0.85f + rnd() * 0.35f, 0, (i * 3 + 1) % 4});
    // Three big dancers up front: both hands high, a sideways lean, a fist pump.
    constexpr int FrontStyles[] = {2, 3, 1};
    constexpr float FrontWidth[] = {1.2f, 0.9f, 1.3f};
    for (int i = 0; i < 3; ++i)
        people_.push_back({20.0f + i * 44.0f + (rnd() - 0.5f) * 6.0f, i * 2.1f, 1.5f + rnd() * 0.1f, FrontWidth[i], 1, FrontStyles[i]});
    water_.clear();
}

void AudioBlockPartyScene::draw_person(const Person& p, float bob, int arms)
{
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    const float s = p.scale;
    const float headY = (p.row == 0 ? 97.0f : 106.0f) - bob;
    auto blob = [&](float cx, float cy, float rx, float ry) {
        for (int y = static_cast<int>((cy - ry) * sy) - 1; y <= static_cast<int>((cy + ry) * sy) + 1; ++y)
            for (int x = static_cast<int>((cx - rx) * sx) - 1; x <= static_cast<int>((cx + rx) * sx) + 1; ++x) {
                if (x < 0 || y < 0 || x >= W || y >= H)
                    continue;
                const float nx = ((x + 0.5f) / sx - cx) / rx, ny = ((y + 0.5f) / sy - cy) / ry;
                if (nx * nx + ny * ny <= 1.0f)
                    crowd_[static_cast<size_t>(y * W + x)] = static_cast<uint8_t>(1 + p.row);
            }
    };
    auto limb = [&](float x0, float y0, float x1, float y1, float r) {
        const int n = static_cast<int>(len(x1 - x0, y1 - y0) * 2.0f) + 1;
        for (int i = 0; i <= n; ++i) {
            const float t = static_cast<float>(i) / n;
            blob(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, r, r);
        }
    };
    const float sway = p.style == 3 ? std::sin(time_ * 2.0f + p.phase) * (p.row == 1 ? 5.0f : 3.0f) : 0.0f;  // leans from the hips
    const float hx = p.x + sway;
    blob(hx, headY, 2.9f * s, 3.3f * s);                             // head
    blob(p.x + sway * 0.85f, headY + 4.0f * s, 1.6f * s, 1.5f * s);  // neck
    // Shoulders and body to the bottom of the frame.
    for (float y = headY + 4.5f * s; y < 130.0f; y += 0.5f) {
        const float t = std::min(1.0f, (y - headY - 4.5f * s) / (2.2f * s));
        const float half = (2.2f + 4.6f * p.width * std::sqrt(t)) * s;
        blob(p.x + sway * std::max(0.0f, 1.0f - (y - headY) / (24.0f * s)), y, half, 0.6f);
    }
    // Hands in the air: pumping with the tempo, each dancer a little out of
    // step; the swaying dancers wave one hand side to side instead.
    const float pump = (p.row == 1 ? 6.5f : 3.5f) * std::max(0.0f, std::sin(pumpClock_ + p.phase));
    const float wave = p.style == 3 ? 3.0f * std::sin(time_ * 3.0f + p.phase) : 0.0f;
    const float shoulder = 4.0f + 2.0f * p.width;
    const float reach = p.style == 2 ? 10.5f : 7.0f;  // the both-hands dancer reaches high above the crowd
    if (arms >= 1) {
        const float bx = p.x + sway * 0.7f + shoulder * s;
        limb(bx, headY + 6.5f * s, bx + 5.0f * s + wave, headY - reach * s + pump, 1.3f * s);
        blob(bx + 5.0f * s + wave, headY - (reach + 0.5f) * s + pump, 1.8f * s, 1.8f * s);
    }
    if (arms >= 2) {
        const float bx = p.x + sway * 0.7f - shoulder * s;
        limb(bx, headY + 6.5f * s, bx - 5.0f * s, headY - (reach - 0.5f) * s + pump, 1.3f * s);
        blob(bx - 5.0f * s, headY - reach * s + pump, 1.8f * s, 1.8f * s);
    }
}

bool AudioBlockPartyScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    if (static_cast<int>(backdrop_.size()) != W * H)
        init_world();
    spectrum_.update(audio, dt, 0.04f, 0.22f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float snare = feature(audio, AudioProtocol::Feature::Snare);
    const float bass = spectrum_.band(0.0f, 0.18f);
    const float energy = spectrum_.level;
    if (events_.beatHit) {
        beat_.hit(0.6f + 0.4f * kick);
        offbeat_.v = 0.0f;
        ++beats_;
    }
    if (events_.onsetHit && kick > 0.35f)
        kick_.hit(kick);
    if (events_.onsetHit && snare > 0.4f)
        snare_.hit(snare);
    if (events_.dropHit) {
        drop_.hit();
        palette_.next();
    }
    if (events_.sectionHit)
        palette_.next();
    beat_.decay(dt, 0.18f);
    kick_.decay(dt, 0.12f);
    snare_.decay(dt, 0.12f);
    drop_.decay(dt, 3.0f);
    palette_.update(dt, 1.5f);
    tv_ += dt;
    // The tempo can be unknown (NaN) before the analyser locks on; clamp() would pass NaN through.
    const float rawBpm = audio.feature(AudioProtocol::Feature::Bpm);
    const float bpm = std::isfinite(rawBpm) ? std::clamp(rawBpm, 70.0f, 180.0f) : 90.0f;
    pumpClock_ = std::fmod(pumpClock_ + dt * Tau * bpm / 60.0f, Tau * 64.0f);

    // Speaker cones: a damped spring pushed by the kick and the bass.
    coneVel_ += (kick_.v * 1.4f + bass * 0.5f) * 50.0f * dt;
    coneVel_ += (-cone_ * 280.0f - coneVel_ * 16.0f) * dt;
    cone_ += coneVel_ * dt;

    buf_.px = backdrop_;
    auto R = [&](float x0, float y0, float x1, float y1, const RGB& c) {
        rect(buf_, static_cast<int>(x0 * sx), static_cast<int>(y0 * sy), static_cast<int>(x1 * sx), static_cast<int>(y1 * sy), c);
    };

    // --- windows: warm rooms with neighbours nodding along, one TV glow ---------------------------
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 2; ++r) {
            const float x0 = WindowX[c], y0 = WindowY[r];
            R(x0 - 1.0f, y0 - 1.0f, x0 + WindowW + 1.0f, y0, RGB{150, 120, 100});                      // lintel
            R(x0 - 1.0f, y0 + WindowH, x0 + WindowW + 1.0f, y0 + WindowH + 1.0f, RGB{150, 120, 100});  // sill
            if (window_lit(c, r)) {
                for (float y = y0; y < y0 + WindowH; y += 1.0f / sy) {
                    const float t = (y - y0) / WindowH;
                    R(x0, y, x0 + WindowW, y + 1.0f / sy, lerp(RGB{255, 190, 110}, RGB{190, 110, 60}, t) * (0.85f + 0.15f * beat_.v));
                }
                if (window_dancer(c, r)) {
                    const float bob = beat_.v * 1.5f;
                    const float hx = x0 + WindowW * 0.5f + std::sin(time_ * 1.7f + c) * 1.5f;
                    buf_.disc(hx * sx, (y0 + WindowH - 7.0f + bob) * sy, 3.0f * sx, RGB{40, 20, 20}, false);
                    R(hx - 4.5f, y0 + WindowH - 3.5f + bob, hx + 4.5f, y0 + WindowH, RGB{40, 20, 20});
                }
            }
            else {
                const float flick = 0.6f + 0.4f * std::sin(tv_ * 7.0f + std::sin(tv_ * 2.3f) * 3.0f);
                R(x0, y0, x0 + WindowW, y0 + WindowH, RGB{30, 40, 90} * (0.5f + 0.5f * flick));
            }
            R(x0 + WindowW * 0.5f - 0.5f, y0, x0 + WindowW * 0.5f + 0.5f, y0 + WindowH, RGB{60, 40, 36});  // mullion
        }
    glow(buf_, 124.0f * sx, 22.0f * sy, 10.0f * sx, RGB{120, 90, 50});
    buf_.disc(123.0f * sx, 22.5f * sy, 1.5f * sx, RGB{255, 220, 160}, false);

    // --- string lights: every bulb follows a band, a chase runs on beats ----------------------------
    int bulb = 0;
    for (const auto& s : Strands) {
        for (float x = s.x0; x <= s.x1; x += 0.6f)
            buf_.set(static_cast<int>(x * sx), static_cast<int>(strand_y(s, x) * sy), RGB{20, 14, 18});
        for (float x = s.x0 + 4.0f; x < s.x1; x += 8.0f, ++bulb) {
            const float y = strand_y(s, x) + 1.5f;
            const int group = (bulb / 3) % 12;  // neighbours rise and fall together
            const float band = spectrum_.band(group / 12.0f, (group + 1) / 12.0f);
            const float chase = std::max(0.0f, std::cos((x / 128.0f - time_ * 0.8f) * Tau * 2.0f));
            const RGB c = hsv(palette_.value + (bulb % 4) * 70.0f, 0.65f, 1.0f);
            const float accent = group % 4 == beats_ % 4 ? 1.2f * beat_.v : 0.0f;  // a group swells on each beat
            const float lv = accent + 0.35f + 0.55f * band + 0.25f * chase * beat_.v + 0.3f * drop_.v;
            buf_.disc(x * sx, y * sy, (1.5f + 0.8f * accent) * sx, c * std::min(1.0f, 0.3f + lv), false);
            glow(buf_, x * sx, y * sy, 6.0f * sx, c * (0.35f * lv));
        }
    }

    // --- speaker stack, DJ table --------------------------------------------------------------------
    for (int cab = 0; cab < 2; ++cab) {
        const float top = 56.0f + cab * 17.0f;
        R(2.0f, top, 24.0f, top + 16.0f, RGB{18, 16, 20});
        // Cabinet edges catch a flash of the party light on every kick.
        R(2.0f, top, 2.8f, top + 16.0f, hsv(palette_.value, 0.6f, 1.0f) * (0.15f + 0.7f * kick_.v));
        R(23.2f, top, 24.0f, top + 16.0f, hsv(palette_.value, 0.6f, 1.0f) * (0.15f + 0.7f * kick_.v));
        R(2.0f, top, 24.0f, top + 1.0f, RGB{50, 46, 50});
        for (int s = 0; s < 2; ++s) {
            const float cx = 8.0f + s * 10.0f, cy = top + 8.5f;
            const float push = std::clamp(cone_ * 5.0f, -0.5f, 1.4f);
            const float r = 3.8f + push * 1.6f;
            for (int y = static_cast<int>((cy - r - 1) * sy); y <= static_cast<int>((cy + r + 1) * sy); ++y)
                for (int x = static_cast<int>((cx - r - 1) * sx); x <= static_cast<int>((cx + r + 1) * sx); ++x) {
                    const float d = len((x + 0.5f) / sx - cx, (y + 0.5f) / sy - cy) / r;
                    if (d > 1.0f)
                        continue;
                    const float ring = 0.5f + 0.5f * std::cos(d * 14.0f - push * 3.0f);
                    RGB c = RGB{30, 28, 34} + RGB{70, 66, 76} * (ring * 0.4f) + RGB{255, 190, 130} * (0.35f * ring * kick_.v);
                    if (d < 0.3f)
                        c = RGB{90, 86, 96} * (0.7f + 0.5f * std::max(0.0f, push)) + RGB{255, 200, 140} * (0.8f * kick_.v);
                    if (d > 0.88f)
                        c = lerp(RGB{100, 96, 106}, RGB{255, 200, 150}, kick_.v);  // the rim flashes on kicks
                    buf_.set(x, y, c);
                }
        }
    }
    glow(buf_, 13.0f * sx, 92.0f * sy, 16.0f * sx, RGB{255, 170, 100} * (0.55f * kick_.v));  // the stack lights the pavement on kicks
    R(6.0f, 52.0f, 20.0f, 56.0f, RGB{30, 28, 34});                                           // horn on top
    R(8.0f, 53.0f, 18.0f, 55.0f, RGB{70, 66, 74} * (0.7f + 0.6f * snare_.v));

    // DJ behind the table: nodding head, headphones, one hand up on drops.
    {
        glow(buf_, 36.0f * sx, 70.0f * sy, 22.0f * sx, RGB{255, 150, 70} * (0.55f + 0.35f * beat_.v));  // warm backlight
        glow(buf_, 30.0f * sx, 90.0f * sy, 26.0f * sx, RGB{255, 140, 60} * (0.35f + 0.3f * beat_.v));   // pool on the pavement
        const float nod = beat_.v * 2.6f;
        const float dx = 36.0f;
        // Warm rim around the DJ so the silhouette stands out against the wall.
        R(dx - 6.0f, 68.0f + nod * 0.4f, dx + 6.0f, 80.0f, RGB{230, 140, 70} * (0.6f + 0.3f * beat_.v));
        buf_.disc(dx * sx, (65.0f + nod) * sy, 4.0f * sx, RGB{230, 140, 70} * (0.6f + 0.3f * beat_.v), false);
        R(dx - 5.0f, 69.0f + nod * 0.4f, dx + 5.0f, 80.0f, RGB{16, 12, 20});
        buf_.disc(dx * sx, (65.0f + nod) * sy, 3.0f * sx, RGB{16, 12, 20}, false);
        for (float a = Pi; a <= Tau; a += 0.15f)
            buf_.set(static_cast<int>((dx + std::cos(a) * 3.6f) * sx), static_cast<int>((65.0f + nod + std::sin(a) * 3.6f) * sy),
                     RGB{80, 80, 90});
        buf_.disc((dx - 3.6f) * sx, (65.5f + nod) * sy, 1.2f * sx, RGB{60, 60, 70}, false);
        if (drop_.v > 0.25f) {
            line(buf_, (dx + 5.0f) * sx, 71.0f * sy, (dx + 9.0f) * sx, (60.0f - 2.0f * drop_.v) * sy, RGB{16, 12, 20});
            line(buf_, (dx + 5.5f) * sx, 71.0f * sy, (dx + 9.5f) * sx, (60.0f - 2.0f * drop_.v) * sy, RGB{16, 12, 20});
        }
        // Table with a cloth skirt, two little decks and a mixer glow.
        const RGB cloth = hsv(palette_.value + 180.0f, 0.35f, 0.22f);  // subdued, the performer is the focus
        R(25.0f, 79.0f, 48.0f, 81.0f, RGB{60, 56, 62});
        R(26.0f, 81.0f, 47.0f, 91.0f, cloth);
        for (float x = 26.0f; x < 47.0f; x += 3.0f) R(x, 81.0f, x + 0.6f, 91.0f, cloth * 0.7f);
        for (int d = 0; d < 2; ++d) {
            const float cx = 30.0f + d * 12.0f;
            const float a = time_ * 3.5f + d;
            buf_.disc(cx * sx, 78.5f * sy, 2.8f * sx, RGB{44, 42, 50}, false);
            buf_.disc(cx * sx, 78.5f * sy, 2.0f * sx, RGB{14, 12, 16}, false);
            buf_.set(static_cast<int>((cx + std::cos(a) * 1.6f) * sx), static_cast<int>((78.5f + std::sin(a) * 0.8f) * sy),
                     RGB{230, 230, 240});
        }
        // One hand works the left deck on the beat.
        const float hand = 30.0f + 2.5f * std::sin(pumpClock_);
        line(buf_, (dx - 4.0f) * sx, 71.0f * sy, hand * sx, 77.5f * sy, RGB{16, 12, 20});
        line(buf_, (dx - 4.0f) * sx, 72.0f * sy, hand * sx, 78.0f * sy, RGB{16, 12, 20});
        buf_.disc(hand * sx, 77.8f * sy, 1.2f * sx, RGB{20, 14, 24}, false);
        glow(buf_, hand * sx, 78.0f * sy, 4.0f * sx, RGB{255, 180, 110} * (0.35f + 0.4f * beat_.v));  // deck light on the hands
        glow(buf_, 36.0f * sx, 79.0f * sy, 4.0f * sx, hsv(palette_.value, 0.7f, 1.0f) * (0.3f + 0.4f * beat_.v));
    }

    // --- the hydrant spraying across the street ---------------------------------------------------------
    R(HydrantX - 2.0f, HydrantY, HydrantX + 2.0f, HydrantY + 8.0f, RGB{190, 30, 30});
    R(HydrantX - 3.0f, HydrantY - 1.0f, HydrantX + 3.0f, HydrantY, RGB{210, 40, 40});
    R(HydrantX - 4.0f, HydrantY + 2.0f, HydrantX - 2.0f, HydrantY + 4.0f, RGB{170, 30, 30});
    if (hydrant_->get()) {
        waterClock_ += dt * (70.0f + 260.0f * energy + 300.0f * drop_.v);
        while (waterClock_ > 1.0f && water_.size() < 400) {
            waterClock_ -= 1.0f;
            const float power = 0.7f + 0.5f * energy + 0.4f * kick_.v;
            water_.push_back({HydrantX - 4.0f, HydrantY + 3.0f, -(28.0f + rnd() * 16.0f) * power, -(26.0f + rnd() * 14.0f) * power, 2.0f});
        }
        waterClock_ = std::min(waterClock_, 1.0f);
    }
    for (auto& w : water_) {
        w.vy += 60.0f * dt;
        w.x += w.vx * dt;
        w.y += w.vy * dt;
        w.life -= dt;
        if (w.y > 100.0f && w.vy > 0.0f) {
            w.vy *= -0.15f;
            w.vx *= 0.4f;
            w.life = std::min(w.life, 0.25f);
        }
    }
    std::erase_if(water_, [](const Drop& w) { return w.life <= 0.0f || w.x < -2.0f; });
    for (const auto& w : water_) {
        // Spray catches the string lights.
        const RGB c = lerp(RGB{140, 180, 230}, hsv(palette_.value, 0.4f, 1.0f), 0.3f);
        buf_.splat(w.x * sx, w.y * sy, c * (0.35f * std::min(1.0f, w.life * 4.0f)));
    }
    // A solid jet out of the nozzle before it breaks into droplets.
    if (hydrant_->get()) {
        const float power = 0.7f + 0.5f * energy + 0.4f * kick_.v;
        const float vx = -36.0f * power, vy = -33.0f * power;
        for (float t = 0.0f; t < 0.22f; t += 0.004f) {
            const float x = HydrantX - 4.0f + vx * t, y = HydrantY + 3.0f + vy * t + 30.0f * t * t;
            const float fade = 1.0f - t / 0.22f;
            buf_.splat(x * sx, y * sy, RGB{200, 225, 255} * (0.5f * fade));
            buf_.splat(x * sx, (y + 0.7f) * sy, RGB{150, 190, 240} * (0.3f * fade));
        }
    }
    // Wet street under the spray reflects the lights.
    for (int y = static_cast<int>(96.0f * sy); y < static_cast<int>(108.0f * sy); ++y)
        for (int x = static_cast<int>(60.0f * sx); x < static_cast<int>(110.0f * sx); ++x) {
            const float gx = (x + 0.5f) / sx;
            const float wet = std::exp(-std::pow((gx - 88.0f) / 18.0f, 2.0f));
            const float shimmer = 0.5f + 0.5f * std::sin(gx * 1.3f + time_ * 4.0f + y);
            buf_.add(x, y, hsv(palette_.value + 30.0f, 0.5f, 1.0f) * (0.08f * wet * shimmer * (0.5f + beat_.v)));
        }

    // --- the crowd: silhouettes rim-lit by the bulbs --------------------------------------------------------
    std::fill(crowd_.begin() + static_cast<std::ptrdiff_t>(std::max(0, static_cast<int>(80 * sy)) * W), crowd_.end(), 0);
    for (int pass = 0; pass < 2; ++pass)
        for (const auto& p : people_) {
            if (p.row != pass)
                continue;
            float bob = 0.0f;
            // Front dancers dip their shoulders in a staggered groove with the tempo.
            if (p.row == 1)
                bob = p.style == 1   ? 5.0f * std::fabs(std::sin(pumpClock_ + p.phase)) - 2.0f         // head bob on every beat
                      : p.style == 2 ? 4.0f * std::fabs(std::sin(pumpClock_ * 0.5f + p.phase)) - 1.5f  // deep dip every other beat
                                     : 1.5f * std::sin(pumpClock_ * 0.5f + p.phase);
            else
                switch (p.style) {
                    case 0:
                        bob = beat_.v * 1.6f;
                        break;
                    case 1:
                        bob = beat_.v * 2.6f * (0.5f + 0.5f * std::sin(p.phase));
                        break;
                    case 2:
                        bob = beat_.v * 1.2f;
                        break;
                    default:
                        bob = 0.8f * std::sin(time_ * 3.0f + p.phase);
                        break;
                }
            bob += drop_.v * 4.0f * std::max(0.0f, std::sin(time_ * 9.0f + p.phase));  // jumping after a drop
            int arms = 0;
            if (p.style == 2)
                arms = 2;
            else if (p.style == 3 || (p.style == 1 && p.row == 0 && energy > 0.5f))
                arms = 1;  // the front fist-pumper keeps its arms down and dips its shoulders instead
            if (drop_.v > 0.5f && p.style != 3 && p.row == 1)
                arms = 2;
            draw_person(p, bob, arms);
        }
    const RGB rimC = hsv(palette_.value, 0.45f, 1.0f);
    for (int y = static_cast<int>(80 * sy); y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const uint8_t m = crowd_[static_cast<size_t>(y * W + x)];
            if (!m)
                continue;
            RGB c = m == 2 ? RGB{14, 9, 20} : RGB{36, 24, 46};
            // Rim light along the top and the outer edges.
            const bool edgeTop = y == 0 || crowd_[static_cast<size_t>((y - 1) * W + x)] < m;
            const bool edgeSide =
                (x > 0 && crowd_[static_cast<size_t>(y * W + x - 1)] < m) || (x + 1 < W && crowd_[static_cast<size_t>(y * W + x + 1)] < m);
            // Dancers near the speakers catch a warm flash on every kick.
            const float speakerLight = std::exp(-(x / sx) / 45.0f) * kick_.v;
            if (edgeTop)
                c = c + rimC * (0.5f + 0.3f * beat_.v) + RGB{255, 170, 90} * (0.8f * speakerLight);
            else if (edgeSide)
                c = c + rimC * 0.2f + RGB{255, 170, 90} * (0.5f * speakerLight);
            buf_.at(x, y) = c;
        }

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioBlockPartyScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "block_party";
    d.tags = {"music",          "street",      "dusk",           "urban",        "crowd",      "summer",
              "audio-reactive", "beat-driven", "music-showcase", "style:hiphop", "style:party"};
    d.intensity = 0.55f;
    d.motion = 0.5f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.4f;
    return d;
}
}  // namespace Scenes
