#include "AmbientMusicScenes.h"

namespace Scenes {
using namespace AudioKit;

namespace {
RGB fire_palette(float h)
{
    h = std::clamp(h, 0.0f, 1.0f);
    if (h < 0.22f)
        return RGB{120, 10, 4} * (h / 0.22f);
    if (h < 0.48f)
        return lerp(RGB{120, 10, 4}, RGB{235, 80, 10}, (h - 0.22f) / 0.26f);
    if (h < 0.75f)
        return lerp(RGB{235, 80, 10}, RGB{255, 190, 40}, (h - 0.48f) / 0.27f);
    return lerp(RGB{255, 190, 40}, RGB{255, 245, 200}, (h - 0.75f) / 0.25f);
}
}  // namespace

void AudioCampfireScene::register_properties()
{
    add_property(sensitivity_);
    add_property(fireSize_);
}

bool AudioCampfireScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    if (heat_.empty())
        heat_.assign(static_cast<size_t>(FireW * FireH), 0.0f);
    spectrum_.update(audio, dt, 0.05f, 0.30f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat);
    const float bass = spectrum_.band(0.0f, 0.25f);
    const float energy = spectrum_.level;
    if (events_.beatHit)
        beat_.hit(0.4f + 0.6f * kick);
    if (events_.dropHit)
        drop_.hit();
    beat_.decay(dt, 0.20f);
    drop_.decay(dt, 1.3f);

    const int groundY = static_cast<int>(H * 0.82f);
    const int fireX0 = W / 2 - FireW / 2;
    const int fireY0 = groundY - FireH + 4;

    auto spawnSpark = [&](float speed) {
        sparks_.push_back({W * 0.5f + (rnd() - 0.5f) * 14.0f, static_cast<float>(groundY - 8 - rnd() * 10), (rnd() - 0.5f) * 14.0f,
                           -(25.0f + rnd() * 25.0f) * speed, 0.0f, 0.8f + rnd() * 1.1f});
    };
    if (events_.onsetHit)
        for (int i = 0; i < 3 + static_cast<int>(hihat * 4); ++i) spawnSpark(1.0f);
    if (events_.dropHit)
        for (int i = 0; i < 40; ++i) spawnSpark(1.6f);
    if (rnd() < dt * (1.5f + hihat * 10.0f))
        spawnSpark(0.8f);
    if (sparks_.size() > 200)
        sparks_.erase(sparks_.begin(), sparks_.begin() + static_cast<std::ptrdiff_t>(sparks_.size() - 200));

    // --- fire simulation (classic heat propagation, two steps per frame) ---
    const float drive = std::clamp(0.50f + bass * 0.40f + beat_.v * 0.35f + drop_.v * 0.4f, 0.0f, 1.4f);
    const float decay = 0.024f / (fireSize_->get() * (0.75f + 0.45f * energy + 0.25f * beat_.v + 0.3f * drop_.v));
    const float wind = std::clamp(audio.feature(AudioProtocol::Feature::StereoBalance), -1.0f, 1.0f) * 0.6f + std::sin(time_ * 0.7f) * 0.3f;
    for (int step = 0; step < 2; ++step) {
        for (int x = 0; x < FireW; ++x) {
            const float u = (x + 0.5f) / FireW * 2.0f - 1.0f;
            const float bell = std::exp(-u * u * 3.0f);
            heat_[static_cast<size_t>((FireH - 1) * FireW + x)] = std::clamp(drive * bell * (0.75f + 0.5f * rnd()), 0.0f, 1.2f);
        }
        for (int y = 0; y < FireH - 1; ++y)
            for (int x = 0; x < FireW; ++x) {
                const float r = rnd();
                const int sx = std::clamp(x + static_cast<int>(std::round((r - 0.5f) * 2.0f + wind * r)), 0, FireW - 1);
                const float below = heat_[static_cast<size_t>((y + 1) * FireW + sx)];
                heat_[static_cast<size_t>(y * FireW + x)] = std::max(0.0f, below - decay * (0.4f + 1.2f * rnd()));
            }
    }
    float fireMass = 0.0f;
    for (float h : heat_) fireMass += h;
    fireMass /= static_cast<float>(FireW * FireH);
    glow_ += (fireMass * 3.0f - glow_) * (1.0f - std::exp(-dt / 0.08f));

    // --- sky, stars, treeline ------------------------------------------
    for (int y = 0; y < groundY; ++y) {
        const float t = static_cast<float>(y) / groundY;
        const RGB c = lerp(RGB{3, 4, 16}, RGB{16, 14, 40}, t);
        for (int x = 0; x < W; ++x) buf_.at(x, y) = c;
    }
    const float treble = spectrum_.band(0.65f, 1.0f);
    for (int i = 0; i < 80; ++i) {
        const int sx = static_cast<int>(hash(i * 3 + 7) * W), sy = static_cast<int>(hash(i * 19 + 1) * groundY * 0.55f);
        const float tw = 0.35f + 0.65f * std::fabs(std::sin(time_ * (0.5f + hash(i) * 1.8f) + i));
        buf_.add(sx, sy, RGB{150, 150, 185} * (tw * (0.4f + treble * 0.7f)));
    }
    // Pine silhouettes, lit faintly from below by the fire.
    for (int t = 0; t < 14; ++t) {
        const float tx = hash(t * 13 + 2) * W;
        const float th = 22.0f + hash(t * 7 + 5) * 26.0f;
        const float tw = th * 0.32f;
        const float base = groundY + 1.0f;
        const float lit = std::clamp(1.0f - std::fabs(tx - W * 0.5f) / (W * 0.55f), 0.0f, 1.0f) * glow_;
        for (int y = static_cast<int>(base - th); y < static_cast<int>(base); ++y) {
            const float k = (y - (base - th)) / th;
            const float half = tw * k * (0.75f + 0.25f * std::sin(k * 18.0f));
            for (int x = static_cast<int>(tx - half); x <= static_cast<int>(tx + half); ++x)
                buf_.set(x, y, RGB{3, 8, 6} + RGB{60, 25, 8} * (lit * k * 0.35f));
        }
    }

    // --- ground with firelight pool -----------------------------------
    for (int y = groundY; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const float d = AudioKit::len((x + 0.5f - W * 0.5f) * 0.55f, (y - groundY) * 1.6f);
            const float light = glow_ * std::exp(-d / 22.0f);
            buf_.at(x, y) = RGB{8, 7, 6} + RGB{170, 70, 18} * light;
        }
    // Stone ring.
    for (int s = 0; s < 9; ++s) {
        const float a = Pi * (0.05f + 0.9f * s / 8.0f);
        const float sx = W * 0.5f + std::cos(a) * 22.0f;
        const float sy = groundY + 3.0f + std::sin(a) * 4.0f;
        buf_.disc(sx, sy, 2.2f, RGB{55, 48, 44} + RGB{140, 60, 15} * (glow_ * 0.5f), false);
    }

    // --- flames -----------------------------------------------------------
    for (int y = 0; y < FireH; ++y)
        for (int x = 0; x < FireW; ++x) {
            const float h = heat_[static_cast<size_t>(y * FireW + x)];
            if (h < 0.04f)
                continue;
            buf_.add(fireX0 + x, fireY0 + y, fire_palette(h) * std::min(1.0f, h * 3.0f));
        }
    // Logs.
    for (int i = -12; i <= 12; ++i) {
        const int x = W / 2 + i;
        buf_.mix(x, groundY + 1 + i / 5, RGB{60, 30, 14}, 1.0f);
        buf_.mix(x, groundY + 2 + i / 5, RGB{40, 20, 10}, 1.0f);
        buf_.mix(x, groundY + 1 - i / 5, RGB{70, 36, 16}, 1.0f);
        buf_.mix(x, groundY + 2 - i / 5, RGB{45, 22, 10}, 1.0f);
    }
    // Embers glowing in the logs.
    for (int e = 0; e < 8; ++e)
        buf_.add(W / 2 - 9 + static_cast<int>(hash(e * 5) * 18), groundY + 1 + static_cast<int>(hash(e * 3) * 2),
                 RGB{255, 80, 10} * (0.4f + 0.6f * std::fabs(std::sin(time_ * 2.0f + e))));

    // --- sparks -----------------------------------------------------------
    for (auto& s : sparks_) {
        s.life += dt;
        s.vx += (std::sin(time_ * 3.0f + s.maxLife * 10.0f) * 12.0f + wind * 10.0f - s.vx) * dt;
        s.vy *= std::exp(-dt * 0.6f);
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        const float f = std::clamp(1.0f - s.life / s.maxLife, 0.0f, 1.0f);
        buf_.splat(s.x, s.y, lerp(RGB{200, 30, 5}, RGB{255, 210, 90}, f) * f);
    }
    std::erase_if(sparks_, [](const Spark& s) { return s.life >= s.maxLife; });

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioCampfireScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "campfire";
    d.tags = {"music", "fire", "nature", "night", "audio-reactive", "music-showcase", "calm", "organic", "style:acoustic", "style:rock", "style:chill", "drop-reactive"};
    d.intensity = 0.42f;
    d.motion = 0.50f;
    d.music_affinity = 0.9f;
    d.performance_cost = 0.35f;
    return d;
}
}  // namespace Scenes
