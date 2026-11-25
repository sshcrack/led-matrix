#include <algorithm>
#include <cmath>

#include "HipHopScenes.h"

namespace Scenes {
using namespace AudioKit;

namespace {
// Design space 128x128.
constexpr float Horizon = 74.0f;
constexpr float Curb = 84.0f;
constexpr float GroundY = 115.0f;  // where the tyres touch
constexpr float WheelR = 6.0f;
constexpr float RearX = 38.0f, FrontX = 90.0f;
constexpr float ArchR = 7.6f;
constexpr float Travel = 9.0f;     // suspension travel before the wheels leave the road
constexpr float PalmGap = 58.0f;   // palms and street lamps alternate on this spacing
constexpr float MidSpeed = 16.0f;  // px/s of the palm layer
constexpr float RoadSpeed = 46.0f;

enum Part : uint8_t { None, Paint, Glass, Chrome, Seam, Tail, Head, Well };

// The car at rest, facing right, in design pixels.
Part car_part(float x, float y)
{
    // Wheel arches: dark wells around the wheels.
    const float dr = len(x - RearX, y - (GroundY - WheelR)), df = len(x - FrontX, y - (GroundY - WheelR));
    const bool inArch = (dr < ArchR || df < ArchR) && y > 97.0f;
    if (y >= 93.0f && y < 107.0f && x >= 21.0f && x < 107.0f) {
        if ((x >= 105.0f && y < 95.0f) || (x < 23.0f && y < 94.5f) || (x >= 106.0f && y >= 105.0f) || (x < 22.0f && y >= 105.0f))
            return None;
        if (inArch)
            return Well;
        if (x < 23.5f && y >= 95.0f && y < 99.0f)
            return Tail;
        if (x >= 104.0f && y >= 95.0f && y < 97.5f)
            return Head;
        if (y >= 100.0f && y < 101.0f && x > 25.0f && x < 103.0f)
            return Chrome;
        if (std::fabs(x - 58.5f) < 0.5f && y < 105.0f)
            return Seam;
        if (std::fabs(x - 67.0f) < 1.0f && std::fabs(y - 96.5f) < 0.5f)
            return Chrome;
        return Paint;
    }
    // Bumpers.
    if (y >= 100.0f && y < 105.0f && ((x >= 106.0f && x < 110.0f) || (x >= 18.0f && x < 21.0f)))
        return Chrome;
    // Cabin with a slanted windshield and rear window.
    if (y >= 82.0f && y < 93.0f) {
        const float xl = 45.0f - (y - 83.0f) * 1.05f, xr = 71.0f + (y - 83.0f) * 1.1f;
        if (x < xl || x >= xr)
            return None;
        if (y < 83.5f)
            return Paint;  // roof
        if (x < xl + 1.4f || x >= xr - 1.4f || std::fabs(x - 58.5f) < 1.0f || y >= 91.5f)
            return Chrome;  // pillars and frames
        return Glass;
    }
    return None;
}
}  // namespace

void AudioLowriderScene::register_properties()
{
    add_property(sensitivity_);
    add_property(bounce_);
    add_property(underglow_);
}

void AudioLowriderScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    sky_.assign(static_cast<size_t>(W * H), {});
    const float sunX = 98.0f, sunY = Horizon - 1.0f;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float gx = (x + 0.5f) / sx, gy = (y + 0.5f) / sy;
            RGB c;
            if (gy < Horizon) {
                const float t = gy / Horizon;
                c = t < 0.6f ? lerp(RGB{14, 8, 40}, RGB{96, 26, 92}, t / 0.6f)
                             : lerp(RGB{96, 26, 92}, RGB{250, 128, 60}, (t - 0.6f) / 0.4f);
                // Thin sunset cloud streaks.
                const float streak = std::sin(gy * 1.3f + std::sin(gx * 0.05f) * 2.0f);
                if (gy > 38.0f && gy < 66.0f && streak > 0.92f)
                    c = lerp(c, RGB{255, 120, 150}, 0.35f * (streak - 0.92f) / 0.08f);
                const float ds = len(gx - sunX, gy - sunY);
                if (ds < 11.0f)
                    c = lerp(RGB{255, 214, 120}, RGB{255, 160, 80}, ds / 11.0f);
                else
                    c = c + RGB{255, 140, 70} * (0.35f * std::exp(-(ds - 11.0f) / 14.0f));
            }
            else if (gy < Curb) {
                c = RGB{34, 22, 40};  // far sidewalk in shade
            }
            else if (gy < Curb + 1.0f) {
                c = RGB{70, 54, 66};  // curb lip
            }
            else {
                const float grain = 0.88f + 0.24f * hash(static_cast<uint32_t>(x * 7919 + y * 104729));
                const float t = (gy - Curb) / (128.0f - Curb);
                c = lerp(RGB{40, 30, 44}, RGB{26, 22, 30}, t) * grain;
                // Warm sunset sheen on the road surface towards the sun.
                c = c + RGB{90, 46, 30} * (0.22f * std::exp(-std::fabs(gx - sunX) / 26.0f) * (1.0f - t));
            }
            sky_[static_cast<size_t>(y * W + x)] = c;
        }
    sparks_.clear();
    front_ = {};
    rear_ = {};
}

void AudioLowriderScene::hit(Axle& a, float amount)
{
    a.vel += amount * 10.0f * bounce_->get();
}

void AudioLowriderScene::step(Axle& a, float target, float dt, float x)
{
    // Hydraulic ram: a stiff spring towards the target height, gravity takes
    // over once the wheels have left the road.
    const float above = std::max(0.0f, a.lift - Travel);
    const float accel = -60.0f * (std::min(a.lift, Travel) - target) - 6.0f * a.vel - (above > 0.0f ? 140.0f : 0.0f);
    a.vel += accel * dt;
    a.lift += a.vel * dt;
    if (a.lift < -1.5f) {
        if (a.vel < -45.0f)
            for (int i = 0; i < 10; ++i)
                sparks_.push_back(
                    {x + (rnd() - 0.5f) * 10.0f, 107.5f, -(30.0f + rnd() * 50.0f), -(10.0f + rnd() * 30.0f), 0.3f + rnd() * 0.4f});
        a.lift = -1.5f;
        a.vel = -a.vel * 0.25f;
    }
    a.air = std::max(0.0f, a.lift - Travel);
}

bool AudioLowriderScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    const float sx = W / 128.0f, sy = H / 128.0f;
    if (static_cast<int>(sky_.size()) != W * H)
        init_world();
    spectrum_.update(audio, dt, 0.04f, 0.22f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float bass = spectrum_.band(0.0f, 0.18f);
    const float treble = spectrum_.band(0.6f, 1.0f);
    if (events_.beatHit) {
        beat_.hit();
        // Hitting the switches: front, back, front, back; a hard kick lifts both.
        if (kick > 0.7f) {
            hit(front_, 5.0f);
            hit(rear_, 5.0f);
        }
        else if (beatCount_ % 2 == 0)
            hit(front_, 3.0f + 2.0f * kick);
        else
            hit(rear_, 3.0f + 2.0f * kick);
        ++beatCount_;
    }
    if (events_.onsetHit && kick > 0.5f)
        kick_.hit(kick);
    if (events_.dropHit) {
        drop_.hit();
        hit(front_, 17.0f);  // the big front hop
        paint_.next();
    }
    beat_.decay(dt, 0.2f);
    kick_.decay(dt, 0.14f);
    drop_.decay(dt, 1.6f);
    paint_.update(dt, 2.5f);
    const float sit = 0.2f + bass * 2.0f * bounce_->get();
    step(front_, sit, dt, FrontX);
    step(rear_, sit * 0.8f, dt, RearX);

    scrollFar_ += dt * 3.0f;
    scrollMid_ += dt * MidSpeed;
    scrollRoad_ += dt * RoadSpeed;
    wheelAngle_ += dt * RoadSpeed / WheelR;

    // --- sky, far skyline, road ----------------------------------------------------------------
    buf_.px = sky_;
    for (int x = 0; x < W; ++x) {
        const float gx = (x + 0.5f) / sx + scrollFar_;
        const int block = static_cast<int>(std::floor(gx / 9.0f));
        const float hgt =
            3.0f + 9.0f * hash(static_cast<uint32_t>(block * 7 + 3)) + (hash(static_cast<uint32_t>(block)) > 0.8f ? 6.0f : 0.0f);
        for (int y = static_cast<int>((Horizon - hgt) * sy); y < static_cast<int>(Horizon * sy); ++y) {
            buf_.at(x, y) = RGB{44, 16, 52};
            // A few lit windows.
            const int wy = static_cast<int>(y / sy);
            if (wy % 3 == 0 && static_cast<int>(gx) % 3 == 0 &&
                hash(static_cast<uint32_t>(block * 131 + wy * 17 + static_cast<int>(gx))) > 0.72f)
                buf_.at(x, y) = RGB{200, 140, 70};
        }
    }

    // --- palms and street lamps passing behind the car ------------------------------------------
    const RGB palm{16, 6, 24};
    float lampXs[4];
    int lamps = 0;
    for (int k = -1; k < 4; ++k) {
        const float base = std::floor(scrollMid_ / PalmGap) * PalmGap + k * PalmGap;
        const float px = base - scrollMid_;  // screen x of this palm
        const uint32_t id = static_cast<uint32_t>(std::lround(base / PalmGap) + 1000);
        // Palm: a gently curved trunk and drooping fronds.
        const float lean = (hash(id) - 0.5f) * 8.0f;
        const float topY = 28.0f + hash(id * 3) * 8.0f;
        for (float y = topY; y < Curb - 2.0f; y += 0.5f) {
            const float t = (Curb - 2.0f - y) / (Curb - 2.0f - topY);
            const float x = px + lean * t * t;
            const float w = 1.6f - 0.6f * t;
            for (float dx = -w; dx <= w; dx += 0.5f) buf_.set(static_cast<int>((x + dx) * sx), static_cast<int>(y * sy), palm);
        }
        const float tx = px + lean, ty = topY;
        for (int f = 0; f < 7; ++f) {
            const float a = -Pi + f * (Pi / 6.0f) + std::sin(time_ * 1.3f + f + id) * 0.06f;
            const float reach = 10.0f + 3.0f * std::sin(f * 1.7f);
            for (float s = 0; s < 1.0f; s += 0.06f) {
                const float fx = tx + std::cos(a) * reach * s;
                const float fy = ty + std::sin(a) * reach * s * 0.6f + 9.0f * s * s;
                buf_.set(static_cast<int>(fx * sx), static_cast<int>(fy * sy), palm);
                buf_.set(static_cast<int>(fx * sx), static_cast<int>(fy * sy) + 1, palm);
            }
        }
        // Street lamp half way to the next palm.
        const float lx = px + PalmGap * 0.5f;
        if (lamps < 4)
            lampXs[lamps++] = lx;
        for (float y = 46.0f; y < Curb - 1.0f; y += 1.0f) buf_.set(static_cast<int>(lx * sx), static_cast<int>(y * sy), RGB{30, 20, 34});
        for (float x = lx - 5.0f; x <= lx; x += 1.0f) buf_.set(static_cast<int>(x * sx), static_cast<int>(46.0f * sy), RGB{30, 20, 34});
        buf_.disc((lx - 5.0f) * sx, 47.0f * sy, 1.2f * sx, RGB{255, 210, 140}, false);
        glow(buf_, (lx - 5.0f) * sx, 48.0f * sy, 9.0f * sx, RGB{110, 70, 30});
        glow(buf_, (lx - 5.0f) * sx, (Curb - 2.0f) * sy, 11.0f * sx, RGB{60, 40, 20});
    }

    // --- lane markings and the underglow on the road ---------------------------------------------
    {
        const float dashY = 123.0f;
        for (int x = 0; x < W; ++x) {
            const float gx = (x + 0.5f) / sx + scrollRoad_;
            if (std::fmod(gx, 24.0f) < 11.0f)
                for (int y = static_cast<int>(dashY * sy); y < static_cast<int>((dashY + 1.5f) * sy); ++y)
                    buf_.at(x, y) = RGB{140, 120, 90};
        }
    }
    const RGB glowC = hsv(paint_.value + 150.0f, 0.85f, 1.0f);  // contrasts with the paint
    const float glowLevel = underglow_->get() ? 0.35f + 0.65f * std::max(bass, kick_.v) : 0.0f;
    const float liftMid = (front_.lift + rear_.lift) * 0.5f;
    if (glowLevel > 0.0f)
        for (int y = static_cast<int>((GroundY - 6.0f) * sy); y < std::min(H, static_cast<int>((GroundY + 10.0f) * sy)); ++y)
            for (int x = static_cast<int>(16 * sx); x < static_cast<int>(112 * sx); ++x) {
                const float gx = (x + 0.5f) / sx, gy = (y + 0.5f) / sy;
                const float u = (gx - 64.0f) / 46.0f;
                const float a = std::exp(-u * u * u * u * 3.0f) * std::exp(-std::fabs(gy - GroundY) / (2.5f + liftMid * 0.3f));
                buf_.add(x, y, glowC * (0.9f * a * glowLevel));
            }

    // --- headlight beam ahead ----------------------------------------------------------------------
    for (int y = static_cast<int>(100 * sy); y < std::min(H, static_cast<int>(124 * sy)); ++y)
        for (int x = static_cast<int>(108 * sx); x < W; ++x) {
            const float gx = (x + 0.5f) / sx, gy = (y + 0.5f) / sy;
            const float spread = (gx - 108.0f) * 0.55f + 1.0f;
            const float cy = 97.0f - front_.lift + (gx - 108.0f) * 0.45f;
            const float a = std::exp(-std::pow((gy - cy) / spread, 2.0f)) * std::exp(-(gx - 108.0f) / 30.0f);
            buf_.add(x, y, RGB{255, 220, 160} * (0.35f * a));
        }

    // --- the car ----------------------------------------------------------------------------------
    const RGB candy = hsv(paint_.value, 0.85f, 1.0f);
    auto lift_at = [&](float gx) {
        const float u = (gx - RearX) / (FrontX - RearX);
        return rear_.lift + (front_.lift - rear_.lift) * u;
    };
    // Shadow under the car.
    for (int x = static_cast<int>(24 * sx); x < static_cast<int>(104 * sx); ++x)
        for (int r = 0; r < 2; ++r) buf_.mix(x, static_cast<int>((GroundY - 1.0f + r) * sy), RGB{4, 2, 8}, 0.55f - 0.25f * r);
    const float headBob = beat_.v * 1.2f;
    for (int y = static_cast<int>(78 * sy); y < static_cast<int>(GroundY * sy); ++y)
        for (int x = static_cast<int>(16 * sx); x < static_cast<int>(112 * sx); ++x) {
            const float gx = (x + 0.5f) / sx, gy = (y + 0.5f) / sy;
            const float ry = gy + lift_at(gx);  // rest-pose y
            const Part part = car_part(gx, ry);
            if (part == None)
                continue;
            RGB c;
            switch (part) {
                case Paint: {
                    // Candy paint: hot highlight along the beltline, deep at the rocker,
                    // metal flake that glitters with the hats.
                    const float t = std::clamp((ry - 93.0f) / 14.0f, 0.0f, 1.0f);
                    c = candy * (0.95f - 0.6f * t);
                    if (ry < 95.0f)
                        c = lerp(c, RGB{255, 240, 255}, 0.35f);
                    if (ry < 84.0f)
                        c = lerp(c, RGB{255, 220, 200}, 0.25f);
                    const float flake = hash(static_cast<uint32_t>(x * 92821 + y * 68917) + static_cast<uint32_t>(time_ * 6.0f) * 13u);
                    if (flake > 0.93f)
                        c = c + RGB{255, 255, 255} * ((flake - 0.93f) * 6.0f * (0.2f + treble));
                    // Street lamps sliding along the paint.
                    for (int l = 0; l < lamps; ++l) {
                        const float d = gx - (lampXs[l] - 5.0f) - (ry - 93.0f) * 0.4f;
                        c = c + RGB{255, 220, 170} * (0.45f * std::exp(-d * d / 3.0f) * (1.0f - t * 0.5f));
                    }
                    break;
                }
                case Glass: {
                    const float t = (ry - 83.0f) / 9.0f;
                    c = lerp(RGB{120, 50, 110}, RGB{30, 14, 50}, t);
                    // Driver leaning back, nodding on the beat.
                    const float hx = 66.0f, hy = 87.5f + headBob;
                    if (len(gx - hx, ry - hy) < 2.6f || (gx > 62.5f && gx < 70.0f && ry > hy + 2.0f))
                        c = RGB{10, 6, 16};
                    break;
                }
                case Chrome: {
                    const float s = 0.5f + 0.5f * std::sin(gx * 0.35f - scrollMid_ * 0.2f);
                    c = lerp(RGB{110, 100, 120}, RGB{255, 240, 230}, s * s);
                    break;
                }
                case Seam:
                    c = candy * 0.35f;
                    break;
                case Tail:
                    c = RGB{255, 40, 40};
                    break;
                case Head:
                    c = RGB{255, 240, 200};
                    break;
                case Well:
                    c = RGB{8, 4, 12};
                    break;
                case None:
                    break;
            }
            buf_.at(x, y) = c;
        }
    glow(buf_, 21.0f * sx, (97.0f - rear_.lift) * sy, 4.0f * sx, RGB{200, 20, 20} * 0.6f);

    // Neon tube under the rocker.
    if (glowLevel > 0.0f)
        for (float gx = 26.0f; gx < 102.0f; gx += 0.5f) {
            const float l = lift_at(gx);
            const float gy = 107.0f - l;
            const float dr = std::fabs(gx - RearX), df = std::fabs(gx - FrontX);
            if (dr < ArchR || df < ArchR)
                continue;
            buf_.add(static_cast<int>(gx * sx), static_cast<int>(gy * sy), glowC * (0.9f * glowLevel));
        }

    // --- wheels: thin whitewalls, wire rims with a spinning knock-off ----------------------------------
    for (int w = 0; w < 2; ++w) {
        const Axle& a = w == 0 ? rear_ : front_;
        const float cx = (w == 0 ? RearX : FrontX), cy = GroundY - WheelR - a.air;
        for (int y = static_cast<int>((cy - WheelR - 1) * sy); y <= static_cast<int>((cy + WheelR + 1) * sy); ++y)
            for (int x = static_cast<int>((cx - WheelR - 1) * sx); x <= static_cast<int>((cx + WheelR + 1) * sx); ++x) {
                const float dx = (x + 0.5f) / sx - cx, dy = (y + 0.5f) / sy - cy;
                const float d = len(dx, dy);
                if (d > WheelR)
                    continue;
                const float ang = std::atan2(dy, dx);
                RGB c;
                if (d > 4.9f)
                    c = RGB{14, 12, 16};
                else if (d > 3.9f)
                    c = RGB{220, 214, 206};
                else if (d > 1.4f) {
                    const float spoke = 0.5f + 0.5f * std::cos((ang - wheelAngle_) * 12.0f);
                    c = lerp(RGB{90, 84, 96}, RGB{230, 226, 236}, 0.3f + 0.5f * spoke);
                }
                else
                    c = RGB{255, 200, 90};  // gold knock-off
                // Spinner wings.
                const float wing = std::fabs(std::sin(ang - wheelAngle_ * 0.5f));
                if (d < 3.0f && d > 1.0f && wing < 0.25f)
                    c = RGB{255, 210, 110};
                buf_.set(x, y, c);
            }
    }

    // --- sparks from a hard landing ----------------------------------------------------------------
    for (auto& s : sparks_) {
        s.vy += 120.0f * dt;
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        if (s.y > GroundY) {
            s.y = GroundY;
            s.vy *= -0.3f;
        }
        s.life -= dt;
    }
    std::erase_if(sparks_, [](const Spark& s) { return s.life <= 0.0f; });
    for (const auto& s : sparks_) buf_.splat(s.x * sx, s.y * sy, RGB{255, 180, 60} * std::min(1.0f, s.life * 3.0f));

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioLowriderScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "lowrider";
    d.tags = {"music",       "street",      "dusk",           "urban",        "car",        "audio-reactive",
              "beat-driven", "bass-driven", "music-showcase", "style:hiphop", "style:party"};
    d.intensity = 0.5f;
    d.motion = 0.55f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.35f;
    return d;
}
}  // namespace Scenes
