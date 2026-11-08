#include "HorseScenes.h"

#include <shared/matrix/utils/color.h>

using namespace HorseScenes;

// ---------------------------------------------------------------- base
void HorseSceneBase::register_properties()
{
    speed_->label("Speed").description("Animation speed.").group("Animation").step(0.05);
    add_property(speed_);
}

void HorseSceneBase::initialize(int width, int height)
{
    Scene::initialize(width, height);
    set_target_fps(30);
    cv_.resize(width, height);
    t_ = 0.0f;
}

void HorseSceneBase::begin_frame(float& dt)
{
    dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.1f) * speed_->get();
    t_ += dt;
}

void HorseSceneBase::fill_gradient(Rgb top, Rgb mid, Rgb bottom, float mid_at)
{
    for (int y = 0; y < cv_.h; ++y) {
        const float v = static_cast<float>(y) / std::max(1, cv_.h - 1);
        const Rgb c = v < mid_at ? mix(top, mid, v / mid_at) : mix(mid, bottom, (v - mid_at) / (1.0f - mid_at));
        for (int x = 0; x < cv_.w; ++x) {
            float* p = &cv_.px[static_cast<size_t>((y * cv_.w + x) * 3)];
            p[0] = c.r;
            p[1] = c.g;
            p[2] = c.b;
        }
    }
}

void HorseSceneBase::present(rgb_matrix::FrameCanvas* canvas)
{
    for (int y = 0; y < cv_.h; ++y)
        for (int x = 0; x < cv_.w; ++x) {
            const float* p = &cv_.px[static_cast<size_t>((y * cv_.w + x) * 3)];
            canvas->SetPixel(x, y, static_cast<uint8_t>(std::clamp(p[0], 0.0f, 1.0f) * 255.0f),
                             static_cast<uint8_t>(std::clamp(p[1], 0.0f, 1.0f) * 255.0f),
                             static_cast<uint8_t>(std::clamp(p[2], 0.0f, 1.0f) * 255.0f));
        }
}

namespace {
// Rolling hill silhouette height (0..1 of base) from layered sines.
float hill(float x, float seed)
{
    return 0.5f + 0.28f * std::sin(x * 0.045f + seed) + 0.17f * std::sin(x * 0.11f + seed * 2.3f) +
           0.05f * std::sin(x * 0.27f + seed * 4.1f);
}
void hills(Canvas& cv, float scroll, float base_y, float amp, float seed, Rgb col)
{
    for (int x = 0; x < cv.w; ++x) {
        const float top = base_y - hill(x + scroll, seed) * amp;
        for (int y = std::max(0, static_cast<int>(top)); y < cv.h; ++y)
            cv.blend(x, y, col, y == static_cast<int>(top) ? 1.0f - (top - std::floor(top)) : 1.0f);
    }
}
}  // namespace

// ---------------------------------------------------------------- sunset gallop
bool SunsetGallopScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt;
    begin_frame(dt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h);
    const float k = W / 128.0f;
    gait_phase_ = std::fmod(gait_phase_ + dt * gallopFrequency, 1.0f);
    scroll_ += dt * (gallopStride * gallopFrequency * 1.55f) * k;

    fill_gradient({0.10f, 0.08f, 0.30f}, {0.85f, 0.32f, 0.22f}, {1.0f, 0.70f, 0.30f}, 0.62f);
    // Sun with glow low on the horizon.
    const float sx = W * 0.68f, sy = H * 0.58f, sr = 15.0f * k;
    for (int y = 0; y < cv_.h; ++y)
        for (int x = 0; x < cv_.w; ++x) {
            const float dx = x - sx, dy = y - sy;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d < sr * 3.2f)
                cv_.add(x, y, {1.0f, 0.55f, 0.2f}, 0.30f * std::pow(1.0f - d / (sr * 3.2f), 2.0f));
        }
    cv_.ellipse(sx, sy, sr, sr, 0, {1.0f, 0.88f, 0.55f});
    // A few warm clouds.
    for (int i = 0; i < 3; ++i) {
        const float cx = std::fmod(i * 61.0f * k - scroll_ * 0.05f + W * 4, W + 60 * k) - 30 * k;
        cv_.ellipse(cx, H * (0.18f + 0.09f * i), 24 * k, 2.2f * k, 0, {0.95f, 0.45f, 0.4f}, 0.55f);
    }
    hills(cv_, scroll_ * 0.10f, H * 0.78f, 34 * k, 1.0f, {0.50f, 0.20f, 0.30f});
    hills(cv_, scroll_ * 0.30f, H * 0.88f, 30 * k, 4.0f, {0.28f, 0.10f, 0.22f});
    // Ground band.
    const float ground = H * 0.86f;
    for (int y = static_cast<int>(ground); y < cv_.h; ++y)
        for (int x = 0; x < cv_.w; ++x) {
            const float g = (y - ground) / std::max(1.0f, H - ground);
            cv_.blend(x, y, mix({0.16f, 0.06f, 0.12f}, {0.07f, 0.03f, 0.07f}, g), 1.0f);
        }
    // Grass tufts scrolling at ground speed.
    const float tuftSpacing = 11.0f * k;
    const float tuftOffset = std::fmod(scroll_, tuftSpacing);
    for (float x = -tuftOffset; x < W + tuftSpacing; x += tuftSpacing) {
        cv_.capsule(x, ground + 1, 0.4f, x - k, ground - 2.5f * k, 0.2f, {0.22f, 0.10f, 0.14f});
    }
    // Emit dust at actual touchdowns, rather than continuously behind the body.
    const float hx = W * 0.40f, hy = ground + 3.0f * k;
    const float previousPhase = gait_phase_ - dt * gallopFrequency;
    for (float touchdown : gallopOffsets) {
        const float elapsed = gait_phase_ - touchdown - std::floor(gait_phase_ - touchdown);
        if (elapsed < gait_phase_ - previousPhase) {
            const float hip = touchdown < 0.3f ? -8.0f : 7.0f;
            dust_.push_back(
                {hx + (hip + (touchdown < 0.3f ? 7.5f : 6.3f)) * 1.55f * k, hy, -(18 + rnd() * 10) * k, -(3 + rnd() * 4) * k, 1.0f});
        }
    }
    for (auto& d : dust_) {
        d.x += d.vx * dt;
        d.y += d.vy * dt;
        d.vy += 6.0f * dt;
        d.life -= dt * 1.3f;
        cv_.ellipse(d.x, d.y, 2.0f * k * (1.4f - d.life * 0.4f), 1.4f * k, 0, {0.75f, 0.42f, 0.35f}, std::max(0.0f, d.life) * 0.45f);
    }
    std::erase_if(dust_, [](const Dust& d) { return d.life <= 0.0f; });

    horse(cv_, hx, hy, 1.55f * k, 1.0f, gait_phase_, Gait::Gallop, {0.05f, 0.02f, 0.06f}, {0.03f, 0.01f, 0.04f});
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor SunsetGallopScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "horses";
    d.tags = {"horse", "scenery", "sunset", "landscape", "calm"};
    d.intensity = 0.4f;
    d.motion = 0.7f;
    d.music_affinity = 0.2f;
    d.performance_cost = 0.3f;
    return d;
}

// ---------------------------------------------------------------- misty herd
void MistyHerdScene::initialize(int width, int height)
{
    HorseSceneBase::initialize(width, height);
    herd_.clear();
    const float W = static_cast<float>(width);
    for (int i = 0; i < 6; ++i) {
        const float depth = 0.25f + 0.75f * (i / 5.0f);
        herd_.push_back({(i * 0.19f + rnd() * 0.1f) * W, depth, rnd(), 0.6f + rnd() * 0.5f, i % 4 == 3 ? -1 : 1, i == 2 || i == 4});
    }
    std::sort(herd_.begin(), herd_.end(), [](auto& a, auto& b) { return a.depth < b.depth; });
}

bool MistyHerdScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt;
    begin_frame(dt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    mist_ += dt * 4.0f;

    fill_gradient({0.45f, 0.58f, 0.72f}, {0.95f, 0.80f, 0.72f}, {0.98f, 0.90f, 0.80f}, 0.55f);
    // Pale sun disc.
    cv_.ellipse(W * 0.28f, H * 0.34f, 9 * k, 9 * k, 0, {1.0f, 0.95f, 0.82f}, 0.9f);
    hills(cv_, 10.0f, H * 0.62f, 28 * k, 2.0f, {0.62f, 0.66f, 0.74f});
    hills(cv_, 70.0f, H * 0.72f, 26 * k, 5.0f, {0.48f, 0.55f, 0.58f});

    auto mist_band = [&](float y, float a, float spd, float phase) {
        for (int py = std::max(0, static_cast<int>(y - 7 * k)); py < std::min(cv_.h, static_cast<int>(y + 7 * k)); ++py)
            for (int x = 0; x < cv_.w; ++x) {
                const float n = 0.5f + 0.5f * std::sin(x * 0.07f + mist_ * spd + phase) * std::sin(x * 0.023f - mist_ * 0.4f * spd);
                const float fall = 1.0f - std::fabs(py - y) / (7 * k);
                cv_.blend(x, py, {0.96f, 0.92f, 0.88f}, a * n * fall);
            }
    };
    mist_band(H * 0.70f, 0.55f, 0.3f, 0.0f);

    // Meadow, darker toward the viewer.
    for (int y = static_cast<int>(H * 0.72f); y < cv_.h; ++y) {
        const float g = (y - H * 0.72f) / (H * 0.28f);
        for (int x = 0; x < cv_.w; ++x) cv_.blend(x, y, mix({0.55f, 0.62f, 0.50f}, {0.22f, 0.32f, 0.22f}, g), 1.0f);
    }

    for (auto& m : herd_) {
        m.phase = std::fmod(m.phase + dt * walkFrequency * m.speed, 1.0f);
        const float scale = (0.38f + 0.50f * m.depth) * k;
        if (!m.graze)
            m.x += m.dir * dt * (walkStride * walkFrequency * m.speed) * scale;
        if (m.x > W + 30 * k)
            m.x = -30 * k;
        if (m.x < -30 * k)
            m.x = W + 30 * k;
        const float gy = H * (0.69f + 0.24f * m.depth);
        const float haze = 1.0f - m.depth;  // distant horses fade into mist
        const Rgb body = mix({0.17f, 0.12f, 0.10f}, {0.62f, 0.60f, 0.62f}, haze * 0.75f);
        const Rgb mane = mix({0.08f, 0.06f, 0.06f}, {0.55f, 0.55f, 0.58f}, haze * 0.75f);
        horse(cv_, m.x, gy, scale, static_cast<float>(m.dir), m.phase, m.graze ? Gait::Graze : Gait::Walk, body, mane);
    }
    mist_band(H * 0.90f, 0.20f, 0.5f, 2.0f);
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor MistyHerdScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "horses";
    d.tags = {"horse", "scenery", "dawn", "mist", "landscape", "calm"};
    d.intensity = 0.2f;
    d.motion = 0.3f;
    d.music_affinity = 0.05f;
    d.performance_cost = 0.35f;
    return d;
}

// ---------------------------------------------------------------- pegasus sky
void PegasusSkyScene::initialize(int width, int height)
{
    HorseSceneBase::initialize(width, height);
    field_.clear();
    shoot_life_ = 0;
    next_shoot_ = 6.0f;
    for (int i = 0; i < 90; ++i)
        field_.push_back({rnd() * width, rnd() * height, rnd() < 0.15f ? 1.0f : 0.5f, rnd() * 6.28f, 0.6f + rnd() * 1.8f});
}

bool PegasusSkyScene::render(rgb_matrix::FrameCanvas* canvas)
{
    float dt;
    begin_frame(dt);
    const float W = static_cast<float>(cv_.w), H = static_cast<float>(cv_.h), k = W / 128.0f;
    fill_gradient({0.01f, 0.02f, 0.08f}, {0.03f, 0.05f, 0.16f}, {0.08f, 0.07f, 0.22f}, 0.6f);
    // Faint milky band.
    for (int y = 0; y < cv_.h; ++y)
        for (int x = 0; x < cv_.w; ++x) {
            const float d = std::fabs((y - H) - (-0.9f * x) + 0.0f) / 1.35f / (H * 0.3f);
            const float n = 0.5f + 0.5f * std::sin(x * 0.19f) * std::sin(y * 0.23f + t_ * 0.1f);
            if (d < 1.0f)
                cv_.add(x, y, {0.35f, 0.40f, 0.75f}, 0.06f * (1.0f - d) * n);
        }
    for (auto& s : field_) {
        const float tw = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(t_ * s.rate + s.phase));
        const float starSpeed = s.size > 0.5f ? 2.0f : 1.0f;
        const float starX = std::fmod(s.x + W - std::fmod(t_ * starSpeed * k, W), W);
        cv_.add(static_cast<int>(starX), static_cast<int>(s.y), {0.8f, 0.85f, 1.0f}, tw * s.size);
    }
    // A crescent moon gives the sky a focal point distinct from the horse.
    const float moonX = W * 0.78f, moonY = H * 0.23f;
    cv_.ellipse(moonX, moonY, 16 * k, 16 * k, 0, {0.25f, 0.38f, 0.65f}, 0.12f);
    // Draw the crescent directly; no opaque cutout hiding background stars.
    for (int py = std::max(0, static_cast<int>(moonY - 12 * k)); py < std::min(cv_.h, static_cast<int>(moonY + 12 * k + 1)); ++py) {
        for (int px = std::max(0, static_cast<int>(moonX - 12 * k)); px < std::min(cv_.w, static_cast<int>(moonX + 12 * k + 1)); ++px) {
            const float dx = px + 0.5f - moonX, dy = py + 0.5f - moonY;
            const float outer = 12 * k - std::sqrt(dx * dx + dy * dy);
            const float innerX = dx - 5 * k, innerY = dy + 3 * k;
            const float inner = std::sqrt(innerX * innerX + innerY * innerY) - 11.5f * k;
            cv_.blend(px, py, {0.78f, 0.87f, 1.0f}, std::clamp(std::min(outer, inner) + 0.5f, 0.0f, 1.0f));
        }
    }

    // Broad, dim cloud banks drift underneath the airborne silhouette.
    for (int layer = 0; layer < 2; ++layer) {
        const float period = W + 60 * k;
        for (int i = 0; i < 5; ++i) {
            const float cx = std::fmod(i * period / 5 + period - std::fmod(t_ * (2 + layer) * k, period), period) - 30 * k;
            const float cy = H * (0.89f + 0.08f * layer) + std::sin(i * 2.1f) * 4 * k;
            cv_.ellipse(cx, cy, 30 * k, (8 + 3 * layer) * k, -0.08f, layer == 0 ? Rgb{0.13f, 0.18f, 0.32f} : Rgb{0.08f, 0.11f, 0.23f},
                        0.8f);
        }
    }

    // A full beat includes a downstroke below the back and a slower recovery.
    const float beat = std::fmod(t_ / 3.8f, 1.0f);
    auto wingAngle = [](float p) {
        p -= std::floor(p);
        const float u = p < 0.4f ? p / 0.4f : (p - 0.4f) / 0.6f;
        const float ease = 0.5f - 0.5f * std::cos(kPi * u);
        return (p < 0.4f ? 65 - 90 * ease : -25 + 90 * ease) * kPi / 180;
    };
    const float scale = std::min(W, H) / 128.0f * 1.45f;
    const float horseX = W * 0.51f + std::sin(t_ * 0.23f) * 3 * k;
    const float horseY = H * 0.69f + std::cos((beat - 0.20f) * 2 * kPi) * 1.5f * k;
    auto wing = [&](bool far) {
        const float length = far ? 0.85f : 1.0f;
        const float angle = wingAngle(beat);
        const float shorten = 0.925f + 0.075f * std::sin(angle);
        auto point = [&](float dx, float dy, float featherAngle) -> Point {
            dx *= shorten;
            const float ca = std::cos(featherAngle), sa = std::sin(featherAngle);
            return {horseX + (5 + (far ? 2 : 0) + length * (dx * ca - dy * sa)) * scale,
                    horseY + (-20 + (far ? -1.5f : 0) + length * (dx * sa + dy * ca)) * scale};
        };
        auto stroke = [&](Point a, float ar, Point b, float br, Rgb color) {
            cv_.capsule(a.x, a.y, ar * scale, b.x, b.y, br * scale, color);
        };
        const Rgb shade = far ? Rgb{0.30f, 0.41f, 0.63f} : Rgb{0.62f, 0.74f, 0.95f};
        // Outline the whole near wing first; feather lag bends the primaries
        // slightly without changing the rigid shoulder rotation.
        for (int pass = far ? 1 : 0; pass < 2; ++pass) {
            for (int i = 8; i >= 0; --i) {
                const float u = i / 8.0f;
                const float tipAngle = wingAngle(beat - 0.035f * (1 - u));
                const Point root = point(-8 + 8 * u, 2 * u, angle);
                const Point tip = point(-27 + 12 * u, 2 + 10 * u, tipAngle);
                const Rgb color = pass == 0 ? Rgb{0.07f, 0.10f, 0.21f} : mix(shade, {0.86f, 0.92f, 1.0f}, far ? 0 : (1 - u) * 0.3f);
                const float outline = pass == 0 ? 0.65f : 0;
                stroke(root, 2.3f + outline, tip, 0.55f + outline, color);
            }
        }
        const Rgb edge = far ? shade : Rgb{0.40f, 0.52f, 0.80f};
        stroke(point(0, 0, angle), 2.4f, point(-9, 0, angle), 2.2f, edge);
        stroke(point(-9, 0, angle), 2.2f, point(-27, 2, wingAngle(beat - 0.035f)), 0.55f, edge);
    };
    wing(true);
    horse(cv_, horseX, horseY, scale, 1, std::fmod(t_ / 3.8f, 1.0f), Gait::Fly, {0.88f, 0.92f, 1.0f}, {0.40f, 0.55f, 0.76f});
    wing(false);

    // Rare shooting star.
    next_shoot_ -= dt;
    if (next_shoot_ <= 0.0f) {
        shoot_x_ = rnd() * W * 0.6f;
        shoot_y_ = rnd() * H * 0.3f;
        shoot_life_ = 1.0f;
        next_shoot_ = 8.0f + rnd() * 10.0f;
    }
    if (shoot_life_ > 0.0f) {
        shoot_life_ -= dt * 1.6f;
        const float prog = 1.0f - shoot_life_;
        const float hx = shoot_x_ + prog * 45 * k, hy = shoot_y_ + prog * 24 * k;
        cv_.capsule(hx - 14 * k, hy - 7.5f * k, 0.1f, hx, hy, 0.6f, {0.9f, 0.95f, 1.0f}, std::max(0.0f, shoot_life_));
    }
    present(canvas);
    wait_until_next_frame();
    return true;
}

Scenes::SceneDescriptor PegasusSkyScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.family = "horses";
    d.tags = {"horse", "pegasus", "stars", "night", "moon", "fantasy", "calm"};
    d.intensity = 0.15f;
    d.motion = 0.15f;
    d.music_affinity = 0.0f;
    d.performance_cost = 0.25f;
    return d;
}
