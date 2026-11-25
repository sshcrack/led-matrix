#include "AmbientMusicScenes.h"

namespace Scenes {
using namespace AudioKit;

AudioLighthouseScene::AudioLighthouseScene()
{
    Environment::Adapt adapt;
    adapt.time = false;                                     // the beam is the scene; it only reads at night
    adapt.season_variants = {Environment::Season::Winter};  // other seasons barely change a night coast
    adapt.weather_variants = {Environment::Weather::Fog, Environment::Weather::Rain, Environment::Weather::Snow,
                              Environment::Weather::Storm};
    env_ = std::make_unique<Environment::SceneEnvironment>(
        Environment::Defaults{Environment::TimeOfDay::Night, Environment::Weather::Clear, Environment::Season::Summer}, adapt);
}

void AudioLighthouseScene::register_properties()
{
    add_property(sensitivity_);
    add_property(beatsPerTurn_);
    add_property(storm_);
    for (const auto& p : env_->properties()) add_property(p);
}

void AudioLighthouseScene::initialize(int width, int height)
{
    AmbientMusicScene::initialize(width, height);
    precip_.reset();
    env_->snap();
}

bool AudioLighthouseScene::render(rgb_matrix::FrameCanvas* canvas)
{
    AudioState::Snapshot audio;
    float dt = 0;
    canvas->Clear();
    if (!begin_frame(audio, dt))
        return false;
    const int W = matrix_width, H = matrix_height;
    constexpr int Layers = 6;
    if (layerLevel_.empty())
        layerLevel_.assign(Layers, 0.0f);
    spectrum_.update(audio, dt, 0.06f, 0.40f);

    const float kick = feature(audio, AudioProtocol::Feature::Kick);
    if (events_.beatHit)
        beat_.hit(0.4f + 0.6f * kick);
    if (events_.dropHit) {
        drop_.hit();
        if (storm_->get()) {
            lightning_.hit();
            bolt_.clear();
            float x = W * (0.15f + 0.45f * rnd()), y = 0.0f;
            while (y < H * 0.52f) {
                bolt_.emplace_back(x, y);
                x += (rnd() - 0.5f) * 9.0f;
                y += 3.0f + rnd() * 5.0f;
            }
        }
    }
    beat_.decay(dt, 0.25f);
    drop_.decay(dt, 1.5f);
    lightning_.decay(dt, 0.18f);

    // Beam rotation locked to the tempo: one turn every N beats.
    float bpm = audio.feature(AudioProtocol::Feature::Bpm);
    if (bpm < 60.0f || bpm > 200.0f || audio.feature(AudioProtocol::Feature::BeatConfidence) < 0.05f)
        bpm = bpm >= 60.0f && bpm <= 200.0f ? bpm : 100.0f;
    beamAngle_ = std::fmod(beamAngle_ + dt * Tau * (bpm / 60.0f) / static_cast<float>(beatsPerTurn_->get()), Tau);

    // Far waves = highs, near waves = bass.
    for (int k = 0; k < Layers; ++k) {
        const float from = 1.0f - (k + 1.0f) / Layers, to = 1.0f - static_cast<float>(k) / Layers;
        const float target = spectrum_.band(from, to);
        auto& l = layerLevel_[static_cast<size_t>(k)];
        l += (target - l) * (1.0f - std::exp(-dt / (target > l ? 0.10f : 0.6f)));
    }

    const int horizon = static_cast<int>(H * 0.52f);
    const auto& L = env_->update(dt);
    const Environment::Surface surf{&buf_.px[0].r, W, H, 255.0f};
    const float flash = std::max(lightning_.v, L.flash);
    // Overcast and fog lower a grey ceiling over the stars; the sea and the
    // moonlight on it follow.
    const float grey = std::clamp(L.overcast * 0.9f + L.fog * 0.6f, 0.0f, 1.0f);
    const float moonLight = (0.3f + 0.7f * L.moon_light) * (1.0f - 0.85f * L.overcast) * (1.0f - 0.7f * L.fog);
    const float rough = 1.0f + 0.9f * L.storm + 0.35f * std::fabs(L.wind) + 0.25f * L.rain;

    // --- sky ---------------------------------------------------------------
    for (int y = 0; y < horizon; ++y) {
        const float t = static_cast<float>(y) / horizon;
        const RGB c = lerp(lerp(RGB{4, 6, 22}, RGB{28, 34, 70}, t * t), lerp(RGB{14, 16, 24}, RGB{42, 46, 58}, t), grey) +
                      RGB{110, 110, 150} * (flash * 0.6f);
        for (int x = 0; x < W; ++x) buf_.at(x, y) = c;
    }
    Environment::paint_clouds(surf, L, time_, 0.0f, horizon * 0.7f);
    const float treble = spectrum_.band(0.65f, 1.0f);
    const float starsVisible = std::clamp(1.0f - L.clouds * 0.7f - grey * 1.2f, 0.0f, 1.0f);
    for (int i = 0; starsVisible > 0.02f && i < 70; ++i) {
        const int sx = static_cast<int>(hash(i * 5 + 3) * W), sy = static_cast<int>(hash(i * 23 + 7) * horizon * 0.8f);
        const float tw = 0.35f + 0.65f * std::fabs(std::sin(time_ * (0.4f + hash(i) * 1.6f) + i));
        buf_.add(sx, sy, RGB{150, 150, 180} * (tw * (0.35f + treble * 0.7f) * starsVisible));
    }
    const float moonX = W * 0.22f, moonY = H * 0.16f;
    Environment::paint_moon(surf, L, {moonX, moonY}, 5.0f);
    // Storm clouds drag across the moon: only a smudge of it shows.
    if (L.storm > 0.02f)
        for (int y = static_cast<int>(moonY - 14); y <= static_cast<int>(moonY + 14); ++y)
            for (int x = static_cast<int>(moonX - 20); x <= static_cast<int>(moonX + 20); ++x) {
                if (!buf_.inside(x, y))
                    continue;
                const float n = 0.5f + 0.5f * std::sin(x * 0.21f + time_ * 0.6f) * std::sin(y * 0.33f - time_ * 0.4f + x * 0.05f);
                const float edge = std::clamp(1.0f - AudioKit::len((x - moonX) / 20.0f, (y - moonY) / 14.0f), 0.0f, 1.0f);
                buf_.mix(x, y, RGB{20, 21, 28}, std::min(0.92f, L.storm * (0.55f + 0.45f * n) * std::min(1.0f, edge * 3.0f)));
            }
    lightningEnv_.update_and_draw(surf, L, dt, static_cast<float>(horizon));
    for (size_t i = 1; i < bolt_.size() && lightning_.v > 0.05f; ++i) {
        const auto [x0, y0] = bolt_[i - 1];
        const auto [x1, y1] = bolt_[i];
        for (int s = 0; s <= 6; ++s) {
            const float t = s / 6.0f;
            buf_.splat(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, RGB{230, 230, 255} * (lightning_.v * 1.6f));
        }
    }

    // --- sea: far to near layers ---------------------------------------------
    // Wave surfaces of all layers first, so each layer only fills down to
    // where the next (nearer, opaque) layer starts instead of to the bottom.
    std::vector<std::vector<float>> surfaces(Layers, std::vector<float>(static_cast<size_t>(W)));
    for (int k = 0; k < Layers; ++k) {
        const float depth = static_cast<float>(k) / (Layers - 1);  // 0 far .. 1 near
        const float baseY = horizon + 2.0f + std::pow(depth, 1.4f) * (H - horizon - 10.0f);
        const float level = layerLevel_[static_cast<size_t>(k)];
        const float amp = (0.6f + depth * 4.2f) * (0.35f + level * 1.3f + (k == Layers - 1 ? beat_.v * 0.8f : 0.0f)) *
                          (1.0f + (rough - 1.0f) * depth);  // wind raises the near swell, not the horizon
        const float wl = 0.30f - depth * 0.20f;
        const float speed = 0.8f + depth * 1.6f;
        for (int x = 0; x < W; ++x) {
            const float phase = x * wl + time_ * speed + k * 1.7f;
            surfaces[static_cast<size_t>(k)][static_cast<size_t>(x)] =
                baseY - amp * (0.6f * std::sin(phase) + 0.4f * std::sin(phase * 1.9f + 0.6f));
        }
    }
    // Distant water behind the farthest swell (rows above its crests would
    // otherwise keep the previous frame).
    const RGB farWater = lerp(RGB{8, 16, 40}, RGB{14, 18, 28}, grey * 0.8f) + RGB{80, 80, 110} * (flash * 0.2f);
    for (int y = horizon; y < H; ++y)
        for (int x = 0; x < W; ++x) buf_.at(x, y) = farWater;
    auto speedOf = [](int layer) { return 6.0f + 4.0f * static_cast<float>(layer); };  // whitecaps drift towards the shore
    for (int k = 0; k < Layers; ++k) {
        const float depth = static_cast<float>(k) / (Layers - 1);  // 0 far .. 1 near
        const float level = layerLevel_[static_cast<size_t>(k)];
        const RGB water = lerp(lerp(RGB{10, 22, 52}, RGB{16, 48, 86}, depth), lerp(RGB{16, 22, 32}, RGB{28, 38, 50}, depth), grey * 0.8f) +
                          RGB{80, 80, 110} * (flash * 0.25f);
        const auto& surface = surfaces[static_cast<size_t>(k)];
        for (int x = 0; x < W; ++x) {
            const float sy = surface[static_cast<size_t>(x)];
            const float slope = surface[static_cast<size_t>(std::min(x + 1, W - 1))] - surface[static_cast<size_t>(std::max(x - 1, 0))];
            const int fillEnd = k + 1 < Layers
                ? std::clamp(static_cast<int>(std::floor(surfaces[static_cast<size_t>(k + 1)][static_cast<size_t>(x)])), horizon, H)
                : H;
            for (int y = std::max(horizon, static_cast<int>(std::floor(sy))); y < fillEnd; ++y) {
                const float below = y - sy;
                RGB c = water * (below < 16.0f ? 0.8f + 0.2f * std::exp(-below / 4.0f) : 0.8f);
                // Moonlight glitter column (negligible beyond 3 widths).
                const float u = (x - moonX) / (3.0f + (y - horizon) * 0.25f);
                // Moonlight breaks into short horizontal glints near the crests.
                const int dash = (x + static_cast<int>(time_ * 3.0f) + y * 2) / 3;
                if (u * u < 9.0f && below < 7.0f &&
                    hash(static_cast<uint32_t>(dash * 73 + y * 977) + static_cast<uint32_t>(time_ * 2.5f) * 131) > 0.70f)
                    c = c + RGB{130, 130, 118} * (std::exp(-u * u) * (1.0f - below / 7.0f) * moonLight);
                buf_.at(x, y) = c;
            }
            // Foam on crests, brighter when the band is loud.
            float crest = std::clamp(0.4f + level * 0.9f - std::fabs(slope) * 0.6f, 0.0f, 1.0f);
            // Storm whitecaps: broken patches of foam on the near swell.
            const bool cap =
                L.storm > 0.05f && depth > 0.3f &&
                hash(static_cast<uint32_t>((x + static_cast<int>(time_ * speedOf(k))) / 3) * 97 + static_cast<uint32_t>(k) * 13) > 0.55f;
            if (cap) {
                crest = std::min(1.0f, crest + L.storm * 0.8f);
                if (sy + 1 >= horizon)
                    buf_.mix(x, static_cast<int>(std::floor(sy)) + 1, RGB{170, 190, 215}, L.storm * 0.55f * depth);
            }
            if (sy >= horizon)
                buf_.mix(x, static_cast<int>(std::floor(sy)), RGB{150, 180, 210} * (0.4f + 0.6f * depth),
                         std::min(1.0f, crest * (0.35f + 0.5f * depth) * (1.0f + 0.5f * L.storm)));
        }
        // A small boat riding the third layer, with a lantern.
        if (k == 2) {
            const float bx = std::fmod(W * 0.42f + time_ * 1.2f, W + 20.0f) - 10.0f;
            const float by = surface[static_cast<size_t>(std::clamp(static_cast<int>(bx), 0, W - 1))];
            for (int i = -4; i <= 4; ++i) buf_.set(static_cast<int>(bx) + i, static_cast<int>(by) - (std::abs(i) < 4 ? 1 : 2), RGB{30, 20, 16});
            for (int i = -3; i <= 3; ++i) buf_.set(static_cast<int>(bx) + i, static_cast<int>(by) - 2, RGB{40, 26, 18});
            // The mast and sail heel over and rock in wind and storms.
            const float heel = (0.12f + 0.5f * L.storm + 0.2f * std::fabs(L.wind)) * std::sin(time_ * 1.7f) + 0.25f * L.storm;
            for (int m = 3; m < 12; ++m) buf_.set(static_cast<int>(bx + heel * m), static_cast<int>(by) - m, RGB{50, 40, 34});
            for (int m = 4; m < 11; ++m)
                for (int s = 1; s <= (11 - m) / 2 + 1; ++s)
                    buf_.mix(static_cast<int>(bx + heel * m) + s, static_cast<int>(by) - m, RGB{150, 145, 130}, 0.8f);
            buf_.add(static_cast<int>(bx) - 3, static_cast<int>(by) - 4, RGB{255, 180, 70});
            buf_.disc(bx - 3, by - 4, 2.2f, RGB{70, 40, 10});
        }
    }

    // --- cliff + lighthouse ------------------------------------------------
    const float lampX = W * 0.87f, lampY = H * 0.17f;
    for (int x = static_cast<int>(W * 0.72f); x < W; ++x) {
        const float t = (x - W * 0.72f) / (W * 0.28f);
        const float top = horizon - 14.0f + (1.0f - std::min(t * 2.5f, 1.0f)) * 16.0f + 1.5f * std::sin(x * 0.9f) + std::sin(x * 0.37f);
        for (int y = static_cast<int>(top); y < H; ++y) {
            const float rock = hash(static_cast<uint32_t>((x / 3) * 131 + (y / 3) * 17));
            const float shade = (0.65f + 0.35f * rock) * (1.0f - 0.4f * static_cast<float>(y - top) / (H - top));
            // Snow settles in a cap on the cliff top, deeper where it is flat.
            const float capDepth = 3.0f + 3.0f * (0.5f + 0.5f * std::sin(x * 0.55f + 1.3f));
            float lying = std::clamp((top + capDepth - y) / 1.5f, 0.0f, 1.0f);
            // Rock steps catch moonlight, and snow in winter: short segments at
            // their own heights and lengths, tapering at the ends.
            const float smoothTop = horizon - 14.0f + (1.0f - std::min(t * 2.5f, 1.0f)) * 16.0f;
            bool onShelf = false;
            for (int band = 0; band < 3 && !onShelf; ++band) {
                const int seg = (x + band * 3) / (5 + band * 2);
                const float pick = hash(static_cast<uint32_t>(seg * 31 + band * 7 + 5));
                if (pick < 0.45f)
                    continue;
                const float stepY = smoothTop + 12.0f + band * 14.0f + (hash(static_cast<uint32_t>(seg * 13 + band)) - 0.5f) * 8.0f;
                const float segPos = static_cast<float>((x + band * 3) % (5 + band * 2)) / (5 + band * 2);
                const float thick = (pick > 0.8f ? 2.0f : 1.0f) * std::clamp(std::sin(segPos * Pi) * 1.6f, 0.0f, 1.0f);
                onShelf = y >= stepY && y < stepY + thick;
            }
            if (onShelf)
                lying = std::max(lying, 0.85f);
            lying *= L.snow_cover;
            const RGB rockColor = lerp(RGB{18, 18, 24} * shade, RGB{110, 118, 140} * (0.85f + 0.15f * rock), lying);
            const RGB moonRim = onShelf ? RGB{28, 32, 46} * (moonLight * (1.0f - lying)) : RGB{};
            buf_.set(x, y, lerp(rockColor, rockColor * 0.7f, L.wetness * 0.5f) + moonRim + RGB{30, 30, 40} * (flash * 0.5f));
        }
    }
    const int towerTop = static_cast<int>(lampY + 5), towerBottom = horizon - 13;
    for (int y = towerTop; y <= towerBottom; ++y) {
        const float t = static_cast<float>(y - towerTop) / (towerBottom - towerTop);
        const int half = static_cast<int>(2.0f + t * 2.5f);
        const bool stripe = ((y - towerTop) / 5) % 2 == 1;
        for (int x = static_cast<int>(lampX) - half; x <= static_cast<int>(lampX) + half; ++x) {
            const float side = (x - (lampX - half)) / std::max(1.0f, 2.0f * half);
            const RGB base = stripe ? RGB{150, 30, 30} : RGB{190, 190, 185};
            buf_.set(x, y, base * (0.35f + 0.65f * (1.0f - side)));
        }
    }
    for (int x = static_cast<int>(lampX) - 4; x <= static_cast<int>(lampX) + 4; ++x) buf_.set(x, towerTop, RGB{40, 40, 46});
    for (int x = static_cast<int>(lampX) - 2; x <= static_cast<int>(lampX) + 2; ++x) buf_.set(x, towerTop - 5, RGB{40, 40, 46});

    // Sea fog drifts in front of the cliff; the beam lights it up below.
    Environment::paint_fog(surf, L, time_, horizon - 26.0f, static_cast<float>(H));
    // A denser bank lies on the horizon and swallows the far sea and the sail.
    if (L.fog > 0.02f)
        for (int y = horizon - 14; y < horizon + 26; ++y)
            for (int x = 0; x < W; ++x) {
                if (!buf_.inside(x, y))
                    continue;
                const float v = std::exp(-std::pow((y - (horizon + 4.0f)) / 11.0f, 2.0f));
                const float drift = 0.75f + 0.25f * std::sin(x * 0.07f + time_ * 0.15f) * std::sin(x * 0.023f - time_ * 0.09f);
                buf_.mix(x, y, RGB{62, 66, 76}, std::min(0.85f, L.fog * v * drift));
            }
    // Heavy rain blurs the horizon line.
    if (L.rain > 0.05f)
        for (int y = horizon - 10; y < horizon + 10; ++y)
            for (int x = 0; x < W; ++x) {
                const float v = 1.0f - std::fabs(static_cast<float>(y - horizon)) / 10.0f;
                buf_.mix(x, y, RGB{34, 37, 48}, L.rain * 0.5f * v * (0.7f + 0.3f * std::sin(x * 0.11f + time_ * 0.8f)));
            }
    // Storm surf bursts white against the foot of the cliff.
    if (L.storm > 0.05f) {
        if (events_.beatHit || rnd() < dt * 1.5f)
            spray_ = 1.0f;
        spray_ = std::max(0.0f, spray_ - dt * 1.4f);
        for (int i = 0; i < 40; ++i) {
            const float a = hash(i * 7 + 3), b = hash(i * 13 + 1);
            const float rise = spray_ * (6.0f + 22.0f * b) * L.storm;
            const float sx = W * 0.72f - 2.0f + (a - 0.6f) * 16.0f * (1.2f - spray_) + rise * L.wind * 0.6f;  // leans with the wind
            const float sy = H - 18.0f - rise + 3.0f * a;
            buf_.splat(sx, sy, RGB{200, 215, 235} * (spray_ * (0.5f + 0.5f * b)));
        }
    }
    if (L.fog > 0.02f)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const float d = AudioKit::len(x + 0.5f - lampX, (y + 0.5f - (lampY - 2)) * 1.4f);
                buf_.add(x, y, RGB{120, 100, 70} * (L.fog * 0.35f * std::exp(-d / 26.0f)));
            }

    // --- rotating beam ---------------------------------------------------
    const float dir = std::sin(beamAngle_);    // horizontal extent
    const float facing = std::cos(beamAngle_);  // towards the viewer
    const float beamPower = 0.55f + 0.25f * beat_.v + 0.3f * drop_.v;
    if (std::fabs(dir) > 0.04f) {
        const float length = 160.0f * std::fabs(dir);
        for (int x = 0; x < W; ++x) {
            const float d = (x - lampX) * (dir > 0 ? 1.0f : -1.0f);
            if (d < 0 || d > length)
                continue;
            const float halfWidth = 1.2f + d * (0.10f + 0.10f * L.fog);  // the cone widens in haze
            const float fade = (1.0f - d / length) * beamPower * (0.55f + 0.45f * std::fabs(dir));
            for (int y = static_cast<int>(lampY - 2 - halfWidth); y <= static_cast<int>(lampY - 2 + halfWidth + 1); ++y) {
                const float edge = 1.0f - std::fabs(y - (lampY - 2)) / (halfWidth + 1.0f);
                if (edge > 0)
                    buf_.add(x, y, RGB{255, 235, 170} * (edge * fade * 0.45f * (1.0f + 0.8f * L.fog + 0.3f * L.rain)));
            }
        }
    }
    const float flare = std::pow(std::max(0.0f, facing), 8.0f);
    buf_.disc(lampX, lampY - 2, 2.0f, RGB{255, 240, 190}, false);
    // A small hot core with a soft halo that swells as the beam faces the
    // viewer (a flat disc read as a second moon).
    {
        const float reach = 5.0f + flare * 7.0f + L.fog * 5.0f;
        const float gain = 0.30f + flare * 0.85f + L.fog * 0.15f;
        for (int y = static_cast<int>(lampY - 2 - reach * 3); y <= static_cast<int>(lampY - 2 + reach * 3); ++y)
            for (int x = static_cast<int>(lampX - reach * 3); x <= static_cast<int>(lampX + reach * 3); ++x) {
                if (!buf_.inside(x, y))
                    continue;
                const float d = AudioKit::len(x + 0.5f - lampX, y + 0.5f - (lampY - 2));
                buf_.add(x, y, RGB{255, 215, 140} * (gain * std::exp(-d * d / (reach * reach)) * 0.55f));
            }
        buf_.disc(lampX, lampY - 2, 1.2f + flare * 1.3f, RGB{255, 250, 225}, false);
    }

    // --- weather ----------------------------------------------------------
    // Fewer flakes than drops: a dense flurry reads as a star field.
    precip_.set_max(static_cast<int>((260.0f - 130.0f * L.snow) * render_quality_scale()));
    const float seaY = static_cast<float>(horizon);
    // Flakes and drops over the open sea fall out of view; only the cliff top
    // catches them.
    precip_.update(dt, L, W, H, [seaY, W, H](float x) { return x > W * 0.76f ? seaY - 12.0f : static_cast<float>(H); });
    precip_.draw(surf, L);

    buf_.blit(canvas);
    return true;
}

SceneDescriptor AudioLighthouseScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "coast";
    d.tags = {"music",          "ocean",          "nature",      "night", "fits:dusk",   "fits:night", "fits:fog",       "fits:storm",
              "audio-reactive", "music-showcase", "beat-driven", "calm",  "style:chill", "style:rock", "style:acoustic", "drop-reactive"};
    d.intensity = 0.40f;
    d.motion = 0.45f;
    d.music_affinity = 0.9f;
    d.performance_cost = 0.35f;
    env_->add_variants(d);
    return d;
}
}  // namespace Scenes
