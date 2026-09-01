#include "AudioParticleScene.h"

#include <shared/matrix/media_artwork_state.h>
#include <shared/matrix/plugin/main.h>
#include <shared/matrix/utils/color.h>

#include <algorithm>
#include <cmath>

namespace Scenes {
namespace {
constexpr float Pi = 3.14159265358979323846f;
float feature(const AudioState::Snapshot& audio, AudioProtocol::Feature id, float gain = 1.0f)
{
    return std::clamp(audio.feature(id) * gain, 0.0f, 1.0f);
}
void hsv(float hue, float saturation, float value, uint8_t& r, uint8_t& g, uint8_t& b)
{
    color::hsv_to_rgb(hue, saturation, std::clamp(value, 0.0f, 1.0f), r, g, b);
}
void addPixel(rgb_matrix::FrameCanvas* canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || y < 0 || x >= canvas->width() || y >= canvas->height())
        return;
    uint8_t oldR = 0, oldG = 0, oldB = 0;
    canvas->GetPixel(x, y, &oldR, &oldG, &oldB);
    canvas->SetPixel(x, y, std::min(255, static_cast<int>(oldR) + r), std::min(255, static_cast<int>(oldG) + g),
                     std::min(255, static_cast<int>(oldB) + b));
}
bool consumeEvent(uint64_t current, uint64_t& seen)
{
    // Event flags remain set in the latest UDP packet and can be observed by
    // multiple matrix renders. Monotonic counters make event consumption
    // exactly-once and also let a desktop restart re-prime cleanly.
    if (current < seen) {
        seen = current;
        return false;
    }
    const bool advanced = current > seen;
    seen = current;
    return advanced;
}
}  // namespace

void AudioParticleFieldScene::register_properties()
{
    useSpotifyArtwork_->label("Use Spotify artwork colors")
        .description("Use current Spotify cover colors when available.")
        .group("Appearance");
    add_property(sensitivity_);
    add_property(particleLimit_);
    add_property(persistence_);
    add_property(gravity_);
    add_property(rainbow_);
    add_property(baseColor_);
    add_property(percussionBursts_);
    add_property(dropExplosion_);
    add_property(useSpotifyArtwork_);
}

void AudioParticleFieldScene::spawn(const AudioState::Snapshot& audio, int count, bool radial, float strength)
{
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float balance = audio.feature(AudioProtocol::Feature::StereoBalance);
    const float width = feature(audio, AudioProtocol::Feature::StereoWidth);
    const float centroid = feature(audio, AudioProtocol::Feature::SpectralCentroid);
    const int effective_limit =
        std::max(100, static_cast<int>(std::lround(particleLimit_->get() * (0.45f + 0.55f * render_quality_scale()))));
    particles_.set_limit(static_cast<size_t>(effective_limit));
    for (int i = 0; i < count && static_cast<int>(particles_.size()) < effective_limit; ++i) {
        Particle p{};
        p.x = std::clamp(matrix_width * (0.5f + balance * 0.28f + (unit(rng_) - 0.5f) * (0.18f + width * 0.72f)), 0.0f,
                         static_cast<float>(matrix_width - 1));
        p.y = radial ? matrix_height * (0.46f + (unit(rng_) - 0.5f) * 0.12f) : static_cast<float>(matrix_height - 1);
        const float angle = radial ? unit(rng_) * 2.0f * Pi : -Pi * (0.18f + unit(rng_) * 0.64f);
        const float speed = (18.0f + unit(rng_) * 55.0f) * (0.45f + strength * 1.55f);
        p.vx = std::cos(angle) * speed + balance * 18.0f;
        p.vy = std::sin(angle) * speed;
        p.life = p.maxLife = persistence_->get() * (0.45f + unit(rng_) * 1.25f);
        p.hue = centroid * 180.0f + unit(rng_) * 110.0f;
        p.size = 1.0f + strength * 2.2f + unit(rng_);
        particles_.try_push(std::move(p));
    }
}

bool AudioParticleFieldScene::render(rgb_matrix::FrameCanvas* canvas)
{
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.05f);
    const auto audio = AudioState::snapshot();
    canvas->Clear();
    if (!audio.fresh())
        return false;

    const float gain = sensitivity_->get();
    const float bass = feature(audio, AudioProtocol::Feature::Bass, gain);
    const float kick = feature(audio, AudioProtocol::Feature::Kick, gain);
    const float snare = feature(audio, AudioProtocol::Feature::Snare, gain);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat, gain);
    const float loudness = feature(audio, AudioProtocol::Feature::LoudnessFast, gain);
    hueTime_ += dt;

    if (!eventsPrimed_) {
        beatSeen_ = audio.beat_counter;
        onsetSeen_ = audio.onset_counter;
        dropSeen_ = audio.drop_counter;
        eventsPrimed_ = true;
    }

    if (percussionBursts_->get() && consumeEvent(audio.beat_counter, beatSeen_)) {
        spawn(audio, 18 + static_cast<int>(kick * 72.0f), true, std::max(kick, 0.30f));
    }
    if (percussionBursts_->get() && consumeEvent(audio.onset_counter, onsetSeen_)) {
        spawn(audio, 4 + static_cast<int>((snare + hihat) * 24.0f), snare > hihat, std::max(snare, hihat));
    }
    if (dropExplosion_->get() && consumeEvent(audio.drop_counter, dropSeen_)) {
        const int effective_limit =
            std::max(100, static_cast<int>(std::lround(particleLimit_->get() * (0.45f + 0.55f * render_quality_scale()))));
        spawn(audio, std::min(260, effective_limit), true, 0.78f);
    }

    spawnAccumulator_ += dt * (6.0f + loudness * 80.0f + hihat * 75.0f);
    while (spawnAccumulator_ >= 1.0f) {
        spawn(audio, 1, false, 0.15f + bass * 0.65f + hihat * 0.30f);
        spawnAccumulator_ -= 1.0f;
    }

    const int effective_limit =
        std::max(100, static_cast<int>(std::lround(particleLimit_->get() * (0.45f + 0.55f * render_quality_scale()))));
    particles_.set_limit(static_cast<size_t>(effective_limit));

    const auto artwork = useSpotifyArtwork_->get() ? MediaArtworkState::snapshot() : MediaArtworkState::Snapshot{};
    const float sideFlow = audio.feature(AudioProtocol::Feature::StereoBalance) * 28.0f;
    const float midFlow = feature(audio, AudioProtocol::Feature::Mid) * 15.0f;
    for (auto& p : particles_) {
        const float flow_acceleration = std::sin(hueTime_ * 1.7f + p.y * 0.07f) * midFlow + sideFlow;
        Particles::integrate(p, dt, flow_acceleration, gravity_->get());
        if (p.x < 0.0f)
            p.x += matrix_width;
        if (p.x >= matrix_width)
            p.x -= matrix_width;
        const float life = Particles::life_ratio(p);
        uint8_t r, g, b;
        if (artwork.valid) {
            const auto c = MediaArtworkState::sample(artwork, p.hue / 360.0f + hueTime_ * 0.025f);
            r = static_cast<uint8_t>(c.r * life);
            g = static_cast<uint8_t>(c.g * life);
            b = static_cast<uint8_t>(c.b * life);
        }
        else if (rainbow_->get())
            hsv(p.hue + hueTime_ * 24.0f, 0.78f, life, r, g, b);
        else {
            const auto c = baseColor_->get();
            r = c.r * life;
            g = c.g * life;
            b = c.b * life;
        }
        const int x = static_cast<int>(std::round(p.x));
        const int y = static_cast<int>(std::round(p.y));
        addPixel(canvas, x, y, r, g, b);
        if (p.size > 1.4f) {
            addPixel(canvas, x + 1, y, r / 2, g / 2, b / 2);
            addPixel(canvas, x - 1, y, r / 2, g / 2, b / 2);
            addPixel(canvas, x, y + 1, r / 2, g / 2, b / 2);
        }
    }
    particles_.erase_if([&](const Particle& p) { return p.life <= 0.0f || p.y > matrix_height + 8.0f || p.y < -matrix_height; });
    return true;
}

std::unique_ptr<Scenes::Scene> AudioParticleFieldSceneWrapper::create()
{
    return std::make_unique<AudioParticleFieldScene>();
}
}  // namespace Scenes

Scenes::SceneDescriptor Scenes::AudioParticleFieldScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "particles";
    d.tags = {"music", "particles", "bursts", "audio-reactive"};
    d.intensity = 0.82f;
    d.motion = 0.86f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.62f;
    d.variants = {
        {"airy",
         "Airy particles",
         "Lower-density trails with restrained gravity",
         {{"particle_limit", 520}, {"persistence", 1.35f}, {"gravity", 8.0f}, {"sensitivity", 0.85f}},
         {"airy", "music"},
         0.58f,
         0.70f,
         1.0f,
         0.48f},
        {"explosive",
         "Explosive particles",
         "Dense percussion bursts and strong drop explosions",
         {{"particle_limit", 1500},
          {"persistence", 0.8f},
          {"gravity", 22.0f},
          {"sensitivity", 1.25f},
          {"percussion_bursts", true},
          {"drop_explosion", true}},
         {"energetic", "music"},
         0.96f,
         0.98f,
         1.0f,
         0.78f},
    };
    return d;
}
