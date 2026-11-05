#include "AmbientMusicScenes.h"

namespace Scenes {
using namespace AudioKit;

void AudioReefScene::register_properties()
{
    add_property(sensitivity_);
    add_property(fishCount_);
    add_property(jellyCount_);
}

void AudioReefScene::init_world()
{
    const int W = matrix_width, H = matrix_height;
    sand_.resize(static_cast<size_t>(W));
    for (int x = 0; x < W; ++x)
        sand_[static_cast<size_t>(x)] = H - 11.0f + 3.0f * std::sin(x * 0.045f + 1.0f) + 1.5f * std::sin(x * 0.13f);
    kelp_.clear();
    constexpr int Strands = 13;
    for (int i = 0; i < Strands; ++i) {
        const float x = 6.0f + (W - 12.0f) * (i + 0.15f * (hash(i * 5) - 0.5f)) / (Strands - 1);
        // Interleave bands so lows and highs are spread over the reef.
        const int band = static_cast<int>((i * 7) % Strands) * 32 / Strands;
        kelp_.push_back({x, 0.0f, hash(i * 3 + 1) * Tau, 85.0f + hash(i * 9) * 50.0f, band, 0.0f});
    }
    fish_.clear();
    for (int i = 0; i < fishCount_->get(); ++i)
        fish_.push_back({rnd() * W, 25.0f + rnd() * 50.0f, 0, 0, i % 3 == 0 ? 200.0f : 28.0f + rnd() * 20.0f, rnd()});
    jellies_.clear();
    for (int i = 0; i < jellyCount_->get(); ++i)
        jellies_.push_back({W * (0.2f + 0.6f * rnd()), 20.0f + rnd() * 60.0f, 0, 4.5f + rnd() * 2.5f, 290.0f + rnd() * 50.0f, 0});
}

bool AudioReefScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (sand_.empty())
        init_world();
    spectrum_.update(audio, dt, 0.08f, 0.35f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat);
    const float loud = feature(audio, AudioProtocol::Feature::LoudnessFast);
    if (events_.beatHit) {
        kick_.hit(0.5f + 0.5f * kick);
        for (auto& j : jellies_) {
            j.vy = -10.0f - kick * 8.0f;  // propel upwards
            j.pulse = 1.0f;
        }
    }
    if (events_.dropHit) {
        drop_.hit();
        scatter_ = 1.0f;
    }
    kick_.decay(dt, 0.22f);
    drop_.decay(dt, 1.5f);
    scatter_ *= std::exp(-dt / 0.9f);

    // Bubbles: from a vent and kelp holdfasts on onsets / hi-hats.
    if (events_.onsetHit || rnd() < dt * (1.0f + hihat * 12.0f)) {
        const auto& k = kelp_[static_cast<size_t>(rnd() * kelp_.size()) % kelp_.size()];
        const bool vent = rnd() < 0.5f;
        const float bx = vent ? W * 0.62f : k.x;
        for (int i = 0; i < (events_.onsetHit ? 3 : 1); ++i)
            bubbles_.push_back({bx + rnd() * 3.0f - 1.5f, sand_[static_cast<size_t>(std::clamp(static_cast<int>(bx), 0, W - 1))] - rnd() * 3.0f,
                                0.6f + rnd() * 1.4f, rnd() * Tau});
    }
    if (bubbles_.size() > 160)
        bubbles_.erase(bubbles_.begin(), bubbles_.begin() + static_cast<std::ptrdiff_t>(bubbles_.size() - 160));

    // --- water column ---------------------------------------------------
    const float bass = spectrum_.band(0.0f, 0.25f);
    // The shimmer terms are separable: per-column, per-row and per-diagonal
    // tables replace ~3 sin() per pixel.
    std::vector<float> sx(static_cast<size_t>(W)), sy(static_cast<size_t>(H)), sd(static_cast<size_t>(W + H));
    for (int x = 0; x < W; ++x) sx[static_cast<size_t>(x)] = std::sin(x * 0.21f + time_ * 0.9f);
    for (int y = 0; y < H; ++y) sy[static_cast<size_t>(y)] = std::sin(y * 0.17f - time_ * 0.7f);
    for (int d = 0; d < W + H; ++d) sd[static_cast<size_t>(d)] = std::sin(d * 0.11f + time_ * 0.5f);
    for (int y = 0; y < H; ++y) {
        const float t = static_cast<float>(y) / H;
        const RGB c = lerp(RGB{8, 70, 105}, RGB{1, 8, 28}, std::pow(t, 0.8f));
        for (int x = 0; x < W; ++x) {
            // Faint drifting caustic shimmer in the upper water.
            const float cs = sx[static_cast<size_t>(x)] + sy[static_cast<size_t>(y)] + sd[static_cast<size_t>(x + y)];
            const float shimmer = smoothstep(2.0f, 2.9f, cs) * (1.0f - t) * (0.35f + bass * 0.5f);
            buf_.at(x, y) = c + RGB{40, 90, 100} * shimmer;
        }
    }
    // God rays, slanted, breathing with the kick.
    for (int r = 0; r < 4; ++r) {
        const float base = W * (0.12f + 0.27f * r) + 10.0f * std::sin(time_ * 0.15f + r * 2.0f);
        const float inten = (0.10f + 0.06f * std::sin(time_ * 0.4f + r * 1.3f)) * (1.0f + kick_.v * 1.4f + drop_.v);
        for (int y = 0; y < H; ++y) {
            const float cx = base + y * 0.38f;
            const float fall = 1.0f - static_cast<float>(y) / H;
            for (int dx = -5; dx <= 5; ++dx) {
                const float a = (1.0f - std::fabs(static_cast<float>(dx)) / 5.5f) * fall * inten;
                buf_.add(static_cast<int>(cx) + dx, y, RGB{120, 200, 210} * a);
            }
        }
    }

    // --- jellyfish (behind the reef) -------------------------------------
    for (auto& j : jellies_) {
        j.vy += (3.0f - j.vy) * (1.0f - std::exp(-dt / 0.8f));  // drag back to a slow sink
        j.y += j.vy * dt;
        j.x += std::sin(time_ * 0.3f + j.hue) * 3.0f * dt;
        j.pulse *= std::exp(-dt / 0.25f);
        if (j.y > H + 12) {
            j.y = -10;
            j.x = W * (0.15f + 0.7f * rnd());
        }
        if (j.y < -14)
            j.y = H + 10;
        const float squash = 1.0f - 0.25f * j.pulse;
        const float rx = j.size * (1.0f + 0.2f * j.pulse), ry = j.size * squash;
        const RGB col = hsv(j.hue + time_ * 4.0f, 0.55f, 1.0f);
        for (int y = static_cast<int>(j.y - ry - 1); y <= static_cast<int>(j.y + 1); ++y)
            for (int x = static_cast<int>(j.x - rx - 1); x <= static_cast<int>(j.x + rx + 1); ++x) {
                const float nx = (x + 0.5f - j.x) / rx, ny = (y + 0.5f - j.y) / ry;
                const float d = nx * nx + ny * ny;
                if (d > 1.0f || ny > 0.15f)
                    continue;
                const float rim = 0.25f + 0.75f * smoothstep(0.4f, 1.0f, d);
                buf_.add(x, y, col * (rim * (0.35f + 0.5f * j.pulse)));
            }
        for (int t = 0; t < 4; ++t) {
            const float tx = j.x - rx * 0.6f + t * rx * 0.4f;
            for (int s = 0; s < static_cast<int>(j.size * 2.4f); ++s) {
                const float wx = tx + std::sin(time_ * 2.5f + s * 0.45f + t) * (0.8f + s * 0.08f);
                buf_.splat(wx, j.y + 1.0f + s, col * (0.30f * (1.0f - s / (j.size * 2.4f))));
            }
        }
    }

    // --- kelp: each strand's height is one spectrum band -----------------
    const float mid = spectrum_.band(0.3f, 0.65f);
    for (auto& k : kelp_) {
        const float target = spectrum_.value[static_cast<size_t>(k.band)];
        k.level += (target - k.level) * (1.0f - std::exp(-dt / (target > k.level ? 0.07f : 0.4f)));
        const float ground = sand_[static_cast<size_t>(std::clamp(static_cast<int>(k.x), 0, W - 1))];
        const float height = 16.0f + k.level * 78.0f;
        const float sway = 1.5f + mid * 4.0f + drop_.v * 3.0f;
        float px = k.x;
        for (int s = 0; s < static_cast<int>(height); ++s) {
            const float t = s / height;
            const float x = k.x + std::sin(time_ * 1.1f + k.phase + s * 0.07f) * sway * t;
            const int y = static_cast<int>(ground - s);
            const float tip = smoothstep(0.65f, 1.0f, t) * k.level;
            const RGB stem = lerp(hsv(k.hue, 0.75f, 0.30f), hsv(k.hue + 20.0f, 0.55f, 0.95f), tip + 0.3f * t);
            buf_.mix(static_cast<int>(std::round(x)), y, stem, 1.0f);
            buf_.mix(static_cast<int>(std::round((x + px) * 0.5f)), y, stem, 0.6f);
            // Alternating leaves every few pixels.
            if (s % 6 == 3 && t < 0.95f) {
                const int dir = (s / 6) % 2 ? 1 : -1;
                for (int l = 1; l <= 3; ++l) buf_.mix(static_cast<int>(std::round(x)) + dir * l, y - l / 2, stem * 0.85f, 0.85f);
            }
            px = x;
        }
    }

    // --- sand + caustics + corals ---------------------------------------
    for (int x = 0; x < W; ++x) {
        const float top = sand_[static_cast<size_t>(x)];
        for (int y = static_cast<int>(top); y < H; ++y) {
            const float cs = std::sin(x * 0.35f + time_ * 1.3f) + std::sin(y * 0.6f - time_ * 0.9f) + std::sin((x - y) * 0.25f + time_);
            const float caustic = smoothstep(1.6f, 2.8f, cs) * (0.4f + bass * 0.8f + kick_.v * 0.6f);
            const float shade = 0.55f + 0.45f * (1.0f - (y - top) / 12.0f);
            buf_.at(x, y) = RGB{70, 62, 42} * shade + RGB{90, 110, 80} * caustic;
        }
    }
    const float snare = feature(audio, AudioProtocol::Feature::Snare);
    for (int c = 0; c < 5; ++c) {
        const float cx = W * (0.08f + 0.21f * c) + 4.0f;
        const float cy = sand_[static_cast<size_t>(std::clamp(static_cast<int>(cx), 0, W - 1))];
        const RGB col = hsv(c % 2 ? 12.0f : 330.0f, 0.75f, 0.55f + 0.35f * snare + 0.2f * kick_.v);
        for (int b = 0; b < 4; ++b)
            buf_.disc(cx + (hash(c * 11 + b) - 0.5f) * 8.0f, cy - 1.5f - hash(c * 7 + b) * 4.0f, 1.4f + hash(c + b * 3) * 1.6f, col, false);
    }

    // --- fish school -----------------------------------------------------
    const float tx = W * 0.5f + std::sin(time_ * 0.23f) * W * 0.32f;
    const float ty = H * 0.42f + std::sin(time_ * 0.37f + 1.0f) * H * 0.18f;
    const float speed = 0.7f + loud * 0.8f;
    for (auto& f : fish_) {
        const float ox = std::cos(f.offset * Tau + time_ * 0.6f) * 12.0f;
        const float oy = std::sin(f.offset * Tau * 2.0f + time_ * 0.4f) * 7.0f;
        float ax = (tx + ox - f.x) * 0.9f, ay = (ty + oy - f.y) * 0.9f;
        // Scatter outward on drops.
        const float dx = f.x - W * 0.5f, dy = f.y - H * 0.45f;
        const float dl = std::max(1.0f, AudioKit::len(dx, dy));
        ax += dx / dl * scatter_ * 260.0f;
        ay += dy / dl * scatter_ * 260.0f;
        if (events_.beatHit)
            f.vx += (f.vx >= 0 ? 1.0f : -1.0f) * 10.0f * kick;
        f.vx += ax * dt * speed;
        f.vy += ay * dt * speed;
        const float v = AudioKit::len(f.vx, f.vy);
        const float vmax = 26.0f * speed + scatter_ * 40.0f;
        if (v > vmax) {
            f.vx *= vmax / v;
            f.vy *= vmax / v;
        }
        f.x += f.vx * dt;
        f.y = std::clamp(f.y + f.vy * dt, 6.0f, H - 18.0f);
        const int dir = f.vx >= 0 ? 1 : -1;
        const RGB body = hsv(f.hue, 0.85f, 0.95f);
        const int fx = static_cast<int>(std::round(f.x)), fy = static_cast<int>(std::round(f.y));
        buf_.set(fx, fy, body);
        buf_.set(fx - dir, fy, body * 0.9f);
        buf_.set(fx - 2 * dir, fy, body * 0.75f);
        buf_.set(fx - 3 * dir, fy - 1, body * 0.55f);
        buf_.set(fx - 3 * dir, fy + 1, body * 0.55f);
        buf_.set(fx + dir, fy, RGB{230, 230, 230});
    }

    // --- bubbles ---------------------------------------------------------
    for (auto& b : bubbles_) {
        b.y -= (10.0f + b.r * 6.0f) * dt;
        b.wobble += dt * 4.0f;
        const float x = b.x + std::sin(b.wobble) * 0.8f;
        if (b.r > 1.3f) {
            buf_.disc(x, b.y, b.r, RGB{90, 150, 170}, true);
            buf_.disc(x, b.y, b.r - 0.9f, RGB{-60, -100, -110}, true);  // hollow centre
            buf_.add(static_cast<int>(x - b.r * 0.4f), static_cast<int>(b.y - b.r * 0.4f), RGB{120, 140, 150});
        }
        else
            buf_.splat(x, b.y, RGB{120, 180, 200});
    }
    std::erase_if(bubbles_, [](const Bubble& b) { return b.y < -3; });

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioReefScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "aquarium";
    d.tags = {"music", "underwater", "aquarium", "nature", "audio-reactive", "calm", "music-showcase", "style:chill", "style:acoustic", "style:party", "drop-reactive"};
    d.intensity = 0.45f;
    d.motion = 0.55f;
    d.music_affinity = 0.9f;
    d.performance_cost = 0.4f;
    return d;
}
}  // namespace Scenes
