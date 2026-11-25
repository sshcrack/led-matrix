#include "AmbientMusicScenes.h"

namespace Scenes {
using namespace AudioKit;

AudioPondScene::AudioPondScene()
{
    Environment::Adapt adapt;
    adapt.time = false;  // the moonlit reflection and the fireflies make it a night scene
    adapt.season_variants = {Environment::Season::Autumn, Environment::Season::Winter};
    adapt.weather_variants = {Environment::Weather::Rain, Environment::Weather::Snow, Environment::Weather::Fog};
    env_ = std::make_unique<Environment::SceneEnvironment>(
        Environment::Defaults{Environment::TimeOfDay::Night, Environment::Weather::Clear, Environment::Season::Summer}, adapt);
}

void AudioPondScene::register_properties()
{
    add_property(sensitivity_);
    add_property(rain_);
    add_property(damping_);
    for (const auto& p : env_->properties()) add_property(p);
}

void AudioPondScene::initialize(int width, int height)
{
    AmbientMusicScene::initialize(width, height);
    flakes_.clear();
    leaves_.clear();
    rings_.clear();
    env_->snap();
}

void AudioPondScene::build_sky()
{
    // The reflected night sky: deep gradient, a moon with halo and stars.
    const int W = matrix_width, H = matrix_height;
    sky_.assign(static_cast<size_t>(W * H), {});
    const float mx = W * 0.68f, my = H * 0.30f, mr = 9.0f;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float t = static_cast<float>(y) / H;
            RGB c = lerp(RGB{8, 14, 38}, RGB{2, 18, 30}, t);
            const float d = AudioKit::len(x + 0.5f - mx, y + 0.5f - my);
            c = c + RGB{90, 110, 150} * (0.55f * std::exp(-d / 16.0f));
            if (d < mr + 1) {
                const float shade = 0.82f + 0.18f * std::clamp(((mx - x) + (my - y)) / (mr * 1.4f), -1.0f, 1.0f);
                c = lerp(c, RGB{215, 212, 192} * shade, std::clamp((mr - d) / 2.5f + 0.4f, 0.0f, 1.0f));  // soft-edged reflection
            }
            const uint32_t id = static_cast<uint32_t>(y * W + x);
            // Reflected stars: few and soft (bright points read as open sky).
            if (hash(id * 3 + 11) > 0.995f && d > mr + 4)
                c = c + RGB{60, 68, 90} * hash(id);
            sky_[static_cast<size_t>(y * W + x)] = c;
        }
}

void AudioPondScene::drop(float cx, float cy, float radius, float strength)
{
    const int W = matrix_width, H = matrix_height;
    const int r = static_cast<int>(std::ceil(radius));
    for (int y = static_cast<int>(cy) - r; y <= static_cast<int>(cy) + r; ++y)
        for (int x = static_cast<int>(cx) - r; x <= static_cast<int>(cx) + r; ++x) {
            if (x < 1 || y < 1 || x >= W - 1 || y >= H - 1)
                continue;
            const float d = AudioKit::len(x - cx, y - cy) / std::max(0.5f, radius);
            if (d >= 1.0f)
                continue;
            h0_[static_cast<size_t>(y * W + x)] -= strength * (0.5f + 0.5f * std::cos(d * Pi));
        }
}

bool AudioPondScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (h0_.empty()) {
        h0_.assign(static_cast<size_t>(W * H), 0.0f);
        h1_ = h0_;
        build_sky();
        pads_ = {{W * 0.22f, H * 0.70f, 9.0f, 0.0f}, {W * 0.33f, H * 0.80f, 6.0f, 1.7f}, {W * 0.84f, H * 0.82f, 8.0f, 3.1f},
                 {W * 0.12f, H * 0.42f, 5.0f, 4.4f}};
        for (int i = 0; i < 7; ++i) flies_.push_back({rnd() * W, rnd() * H * 0.6f, 0, 0, rnd() * Tau});
    }
    spectrum_.update(audio, dt);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float onset = feature(audio, AudioProtocol::Feature::OnsetStrength);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat);
    const float rain = rain_->get();
    if (events_.beatHit) {
        beat_.hit(0.5f + 0.5f * kick);
        // Heavy drop: bigger ripples on the beat, biased towards the moon path.
        drop(W * (0.45f + rnd() * 0.45f), H * (0.15f + rnd() * 0.7f), 2.5f + kick * 2.0f, (1.6f + kick * 2.2f) * rain);
    }
    if (events_.onsetHit)
        drop(rnd() * W, rnd() * H, 1.4f, (0.6f + onset * 1.2f) * rain);
    if (events_.dropHit)
        for (int i = 0; i < 14; ++i) {
            const float a = i / 14.0f * Tau;
            drop(W * 0.5f + std::cos(a) * 34.0f, H * 0.5f + std::sin(a) * 34.0f, 2.0f, 2.0f * rain);
        }
    beat_.decay(dt, 0.25f);
    drizzle_ += dt * rain * (0.4f + hihat * 5.0f + spectrum_.band(0.6f, 1.0f) * 2.0f);
    while (drizzle_ >= 1.0f) {
        drizzle_ -= 1.0f;
        drop(rnd() * W, rnd() * H, 1.2f, 0.35f + rnd() * 0.3f);
    }

    // Real rain patters on the whole pond; snowflakes settle and melt in.
    const auto& L = env_->update(dt);
    // Many small, short-lived impact rings; the large rings stay for beats.
    weatherDrops_ += dt * (L.rain * 30.0f + L.storm * 15.0f);
    while (weatherDrops_ >= 1.0f) {
        weatherDrops_ -= 1.0f;
        rings_.push_back({rnd() * W, rnd() * H, 0.0f});
        if (rnd() < 0.3f)
            drop(rings_.back().x, rings_.back().y, 0.8f, 0.12f);
    }
    for (auto& r : rings_) r.age += dt;
    std::erase_if(rings_, [](const Ring& r) { return r.age >= 0.7f; });
    flakeClock_ += dt * L.snow * 22.0f;
    while (flakeClock_ >= 1.0f && flakes_.size() < 80) {
        flakeClock_ -= 1.0f;
        flakes_.push_back({rnd() * W, rnd() * H, 0.0f, rnd() < 0.15f ? 2.0f : 1.0f});
    }
    for (auto& f : flakes_) {
        f.life += dt;
        f.x += L.wind * 4.0f * dt;
        if (f.life >= 1.6f)
            drop(f.x, f.y, 0.9f, 0.12f);
    }
    std::erase_if(flakes_, [](const Flake& f) { return f.life >= 1.6f; });
    // Autumn leaves land now and then and drift with the wind.
    if (leaves_.size() < 10 && rnd() < dt * L.autumn * 0.5f) {
        const RGB tones[] = {RGB{150, 70, 18}, RGB{175, 120, 25}, RGB{120, 40, 20}};
        leaves_.push_back({rnd() * W, rnd() * H, rnd() * Tau, (rnd() - 0.5f) * 0.4f, tones[static_cast<int>(rnd() * 2.99f)]});
    }
    for (auto& l : leaves_) {
        // Drift slows near the bottom corners, where leaves collect by the reeds.
        const float slow = l.y > H * 0.75f && (l.x < W * 0.3f || l.x > W * 0.75f) ? 0.15f : 1.0f;
        l.x += (L.wind * 3.0f + 0.6f) * dt * slow;
        l.y += 0.8f * dt * slow;
        l.y += std::sin(time_ * 0.3f + l.angle) * 0.6f * dt;
        l.angle += l.spin * dt;
    }
    std::erase_if(leaves_, [W](const Leaf& l) { return l.x > W + 4; });
    if (L.autumn < 0.05f)
        leaves_.clear();

    // Two wave-equation steps per frame (ripple speed ~60 px/s).
    const float damping = damping_->get();
    for (int step = 0; step < 2; ++step) {
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x) {
                const size_t i = static_cast<size_t>(y * W + x);
                const float avg = (h0_[i - 1] + h0_[i + 1] + h0_[i - W] + h0_[i + W]) * 0.25f;
                float n = avg * 2.0f - h1_[i];
                n += (avg - n) * 0.12f;  // viscosity: kills grid-scale noise
                h1_[i] = n * damping;
            }
        // Lily pads float and calm the water under them.
        for (const auto& p : pads_) {
            for (int y = static_cast<int>(p.y - p.r); y <= static_cast<int>(p.y + p.r); ++y)
                for (int x = static_cast<int>(p.x - p.r); x <= static_cast<int>(p.x + p.r); ++x)
                    if (x > 0 && y > 0 && x < W - 1 && y < H - 1 && AudioKit::len(x - p.x, y - p.y) < p.r)
                        h1_[static_cast<size_t>(y * W + x)] *= 0.6f;
        }
        std::swap(h0_, h1_);
    }

    // Shade: refract the sky by the surface gradient, add crest highlights.
    const float tint = spectrum_.band(0.0f, 0.3f);
    // Clouds and fog replace the reflected moon and stars with a dull grey.
    // A faint patch of the moon still shows through.
    const float grey = std::clamp(L.overcast * 0.9f + L.fog * 0.6f + L.clouds * 0.25f, 0.0f, 0.78f);
    const RGB greySky = RGB{20, 26, 34} + RGB{120, 130, 150} * (L.flash * 0.8f);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y * W + x);
            const float gx = h0_[static_cast<size_t>(y * W + std::min(x + 1, W - 1))] - h0_[static_cast<size_t>(y * W + std::max(x - 1, 0))];
            const float gy = h0_[static_cast<size_t>(std::min(y + 1, H - 1) * W + x)] - h0_[static_cast<size_t>(std::max(y - 1, 0) * W + x)];
            // A slow shimmer keeps the reflection from reading as open sky.
            // Stronger near the moon, whose reflection breaks into wavering
            // horizontal bands like moonlight on water.
            const float nearMoon = std::exp(-AudioKit::len(x - W * 0.68f, (y - H * 0.30f) * 0.7f) / 12.0f);
            const float shimmer = std::sin(y * 0.9f + time_ * 1.6f + std::sin(x * 0.05f)) * (0.9f + 2.2f * nearMoon);
            const int sx = std::clamp(static_cast<int>(x + gx * 14.0f + shimmer), 0, W - 1);
            const int sy = std::clamp(static_cast<int>(y + gy * 14.0f), 0, H - 1);
            RGB c = lerp(sky_[static_cast<size_t>(sy * W + sx)], greySky * (0.8f + 0.4f * static_cast<float>(sy) / H), grey);
            // Bright parts of the reflection waver in soft horizontal bands.
            const float bright = std::clamp((c.r - 40.0f) / 90.0f, 0.0f, 1.0f);
            if (bright > 0.0f) {
                const float band = 0.5f + 0.5f * std::sin(y * 1.1f + time_ * 2.0f + std::sin(x * 0.25f + time_ * 0.8f) * 1.8f);
                c = c * (1.0f - bright * 0.32f * (1.0f - band));
            }
            const float slope = (-gx - gy) * 2.2f;
            const float spec = std::pow(std::clamp(slope, 0.0f, 1.0f), 1.8f) * 0.6f;
            c = c * (1.0f - 0.25f * std::clamp(-slope, 0.0f, 1.0f)) + RGB{120, 150, 195} * spec;
            c = c + RGB{10, 40, 60} * (tint * 0.6f + beat_.v * 0.25f);
            buf_.px[i] = c;
        }

    // Lily pads with a notch and a pink flower that opens with the bass. They
    // yellow in autumn and lie brown in winter; flowers only bloom in the warm
    // months.
    const auto padRgb = L.foliage({18.0f / 255.0f, 70.0f / 255.0f, 30.0f / 255.0f}, 0.8f);
    const RGB padColor{padRgb.r * 255.0f, padRgb.g * 255.0f, padRgb.b * 255.0f};
    const float bloom = std::clamp(L.summer + L.spring * 0.6f, 0.0f, 1.0f);
    for (size_t pi = 0; pi < pads_.size(); ++pi) {
        const auto& p = pads_[pi];
        // In autumn each pad turns its own way, from ochre to rust.
        const RGB autumnTones[] = {RGB{70, 72, 24}, RGB{120, 78, 22}, RGB{125, 45, 20}, RGB{95, 85, 30}};  // olive .. rust
        const RGB padTone = lerp(padColor, autumnTones[pi % 4], L.autumn * 0.85f);
        const float bob = std::sin(time_ * 0.8f + p.phase) * 0.4f;
        for (int y = static_cast<int>(p.y - p.r - 1); y <= static_cast<int>(p.y + p.r + 1); ++y)
            for (int x = static_cast<int>(p.x - p.r - 1); x <= static_cast<int>(p.x + p.r + 1); ++x) {
                const float dx = x + 0.5f - p.x, dy = y + 0.5f - (p.y + bob);
                const float d = AudioKit::len(dx, dy);
                float ang = std::atan2(dy, dx) - p.phase;
                ang = std::remainder(ang, Tau);
                if (std::fabs(ang) < 0.32f)
                    continue;
                // Pads die back in winter: fainter, with frost on the rim.
                const float a = std::clamp(p.r - d + 0.5f, 0.0f, 1.0f) * (1.0f - 0.25f * L.winter);
                const float vein = 0.85f + 0.15f * std::cos(ang * 9.0f);
                buf_.mix(x, y, padTone * vein * (0.8f + 0.2f * d / p.r), a);
                if (d > p.r - 1.2f && L.snow_cover > 0.02f && hash(static_cast<uint32_t>(x * 5 + y * 11)) > 0.5f)  // patchy frost
                    buf_.mix(x, y, RGB{150, 160, 180}, a * L.snow_cover * 0.35f);
            }
        if (p.r > 7 && bloom > 0.1f)
            buf_.disc(p.x + 1, p.y + bob, 1.6f + tint * 1.4f, RGB{200, 80, 140} * ((0.6f + beat_.v * 0.5f) * bloom), false);
    }

    // Fireflies drift above the water; they glow with the high end.
    const float treble = spectrum_.band(0.55f, 1.0f);
    // Fireflies need a warm, dry night.
    const float warm = std::clamp(L.summer + L.spring * 0.4f, 0.0f, 1.0f) * (1.0f - L.rain) * (1.0f - L.snow);
    for (auto& f : flies_) {
        f.vx += (std::sin(time_ * 0.7f + f.phase * 3.0f) * 6.0f - f.vx) * dt;
        f.vy += (std::cos(time_ * 0.5f + f.phase * 2.0f) * 4.0f - f.vy) * dt;
        f.x = std::fmod(f.x + f.vx * dt + W, static_cast<float>(W));
        f.y = std::clamp(f.y + f.vy * dt, 4.0f, H - 10.0f);
        const float glow = std::clamp(0.25f + treble * 1.1f + 0.3f * std::sin(time_ * 3.0f + f.phase * 5.0f), 0.0f, 1.2f) * warm;
        buf_.splat(f.x, f.y, RGB{220, 240, 90} * glow);
        buf_.disc(f.x, f.y, 2.0f, RGB{40, 60, 10} * glow);
    }

    // Leaves: a pointed oval with a darker midrib and a stem.
    for (const auto& l : leaves_) {
        const float ca = std::cos(l.angle), sa = std::sin(l.angle);
        for (int v = -5; v <= 5; ++v)
            for (int u = -7; u <= 7; ++u) {
                // Wider on one half and towards the tip: reads as a leaf.
                const float a = u / 3.4f, b = v / (v > 0 ? 3.0f : 2.2f);
                const float lu = u * 0.85f, lv = v * 0.85f;
                const float shape = 1.0f - (a * a / 4.0f + b * b * (1.0f + 0.35f * std::fabs(a) - 0.12f * a));
                if (shape <= 0.0f)
                    continue;
                const float px = l.x + ca * lu - sa * lv, py = l.y + sa * lu + ca * lv;
                const RGB c = std::fabs(v) < 1 ? l.color * 0.55f : l.color * (0.8f + 0.2f * hash(static_cast<uint32_t>(u * 5 + v)));
                buf_.mix(static_cast<int>(px), static_cast<int>(py), c, std::clamp(shape * 3.0f, 0.0f, 0.95f));
            }
        buf_.mix(static_cast<int>(l.x - ca * 6.5f), static_cast<int>(l.y - sa * 6.5f), l.color * 0.45f, 0.9f);
    }
    // Shore banks in the bottom corners (snow lies on them in winter) with
    // reeds that lean, overlap and sway with the mids.
    const float mid = spectrum_.band(0.25f, 0.6f);
    const RGB bankEarth = lerp(RGB{16, 20, 13}, RGB{10, 12, 10}, L.wetness);
    const RGB bankSnow = RGB{120, 128, 150};
    const float moonGlow = 1.0f - grey * 0.8f;
    for (int side = 0; side < 2; ++side) {
        const float cx = side == 0 ? -6.0f : W + 6.0f;
        for (int y = H - 22; y < H; ++y)
            for (int x = side == 0 ? 0 : W - 44; x < (side == 0 ? 44 : W); ++x) {
                const float dx = (x + 0.5f - cx) / 44.0f, dy = (y + 0.5f - (H + 4.0f)) / 20.0f;
                const float wobble = 0.10f * std::sin(x * 0.45f + side * 2.0f) + 0.06f * std::sin(x * 1.3f + 1.0f);
                const float d = dx * dx + dy * dy + wobble;
                if (d >= 1.08f)
                    continue;
                // Cool light on the waterline.
                if (d >= 1.0f) {
                    // Wet in rain, a thin ice rim when snow lies.
                    const RGB edge = RGB{30, 45, 70} * moonGlow + RGB{25, 35, 50} * L.wetness + RGB{70, 85, 105} * L.snow_cover +
                                     RGB{70, 45, 12} * L.autumn;  // fallen leaves gather at the waterline
                    buf_.add(x, y, edge * ((1.08f - d) / 0.08f));
                    continue;
                }
                const float rim = std::clamp((1.0f - d) * 8.0f, 0.0f, 1.0f);
                const float patchy = 0.75f + 0.25f * std::sin(x * 0.7f + y * 1.1f) * std::sin(x * 0.23f - y * 0.4f);
                const float snowTop =
                    std::clamp((0.35f + (d - 0.2f) * 1.6f) * patchy, 0.0f, 1.0f) * L.snow_cover;  // covers the bank, thickest on top
                const RGB earth = bankEarth * (0.8f + 0.4f * hash(static_cast<uint32_t>(x * 13 + y * 7)));
                buf_.mix(x, y, lerp(earth, bankSnow * (0.85f + 0.15f * hash(static_cast<uint32_t>(x * 7 + y * 3))), snowTop), rim);
            }
    }
    // Frost creeps along the near shore between the banks.
    if (L.snow_cover > 0.02f)
        for (int x = 0; x < W; ++x) {
            const float depth = 1.5f + 1.5f * (0.5f + 0.5f * std::sin(x * 0.37f) * std::sin(x * 0.11f + 1.0f));
            for (int y = H - static_cast<int>(depth) - 1; y < H; ++y)
                buf_.mix(x, y, RGB{140, 150, 172}, L.snow_cover * std::clamp(depth - (H - 1 - y), 0.0f, 1.0f) * 0.75f);
        }
    for (int r = 0; r < 14; ++r) {
        const bool left = r < 8;
        const int k = left ? r : r - 8;
        const float bx = left ? 1.0f + k * 3.6f + hash(r * 3) * 2.0f : W - 2.0f - k * 4.2f - hash(r * 3) * 2.0f;
        const float len = 22.0f + hash(r * 17) * 30.0f;
        const float lean = (left ? 1.0f : -1.0f) * (2.0f + hash(r * 29) * 7.0f);
        const float sway = std::sin(time_ * 1.3f + r) * (1.2f + mid * 4.0f);
        const bool thick = hash(r * 41) > 0.5f;
        const RGB stem = RGB{10, 20, 11} * (0.8f + 0.4f * hash(r * 5));
        for (int s = 0; s < static_cast<int>(len); ++s) {
            const float t = s / len;
            const float x = bx + (lean + sway * (left ? 1.0f : -1.0f)) * t * t;
            const int ix = static_cast<int>(x), iy = H - 3 - s;
            buf_.mix(ix, iy, lerp(stem, RGB{95, 62, 18}, L.autumn * std::clamp((t - 0.55f) * 2.5f, 0.0f, 1.0f)), 0.95f);
            if (r % 4 == 1 && t > 0.2f)  // moonlight on a few stalks
                buf_.add(ix + (left ? 1 : -1), iy, RGB{22, 32, 48} * (moonGlow * 0.6f));
            if (thick && t < 0.6f)
                buf_.mix(ix + 1, iy, stem * 0.8f, 0.9f);
            const bool head = r % 3 == 0 && t > 0.72f && t < 0.9f;  // cattail
            if (head) {
                buf_.mix(ix - 1, iy, RGB{34, 18, 7}, 0.95f);
                buf_.mix(ix + 1, iy, RGB{34, 18, 7}, 0.95f);
            }
            // Snow settles on the tips and the cattail tops.
            if (L.snow_cover > 0.02f && (t > 0.93f || (head && t > 0.86f)))
                buf_.mix(ix, iy, bankSnow, L.snow_cover * 0.9f);
        }
    }

    // Raindrop rings: a thin circle that widens and fades.
    for (const auto& r : rings_) {
        const float rad = 0.8f + r.age * 6.0f, a = (1.0f - r.age / 0.7f) * 0.65f;
        if (r.age < 0.12f)  // the impact itself
            buf_.splat(r.x, r.y, RGB{160, 180, 210} * (1.0f - r.age / 0.12f));
        const int n = 10 + static_cast<int>(rad * 5.0f);
        for (int k = 0; k < n; ++k) {
            const float ang = k * Tau / static_cast<float>(n);
            buf_.splat(r.x + std::cos(ang) * rad, r.y + std::sin(ang) * rad,
                       RGB{110, 135, 170} * (a * 6.0f / static_cast<float>(n) * 2.2f));
        }
    }
    // Snowflakes resting on the surface fade as they melt.
    for (const auto& f : flakes_) {
        const float a = std::clamp(1.0f - f.life / 1.6f, 0.0f, 1.0f);
        const int fx = static_cast<int>(f.x), fy = static_cast<int>(f.y);
        // A soft flake that spreads a little as it melts into the water.
        const float spread = f.size * (0.6f + 0.5f * f.life / 1.6f);
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const float w = std::exp(-(dx * dx + dy * dy) / (spread * spread));
                buf_.mix(fx + dx, fy + dy, RGB{175, 183, 205}, a * w * 0.8f);
            }
    }
    const Environment::Surface surf{&buf_.px[0].r, W, H, 255.0f};
    Environment::paint_fog(surf, L, time_, 0.0f, static_cast<float>(H));

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioPondScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "pond";
    d.tags = {"music",          "water", "nature",         "night",       "fits:dusk",      "fits:night",   "fits:rain",
              "audio-reactive", "calm",  "music-showcase", "style:chill", "style:acoustic", "drop-reactive"};
    d.intensity = 0.35f;
    d.motion = 0.45f;
    d.music_affinity = 0.9f;
    d.performance_cost = 0.35f;
    env_->add_variants(d);
    return d;
}
}  // namespace Scenes
