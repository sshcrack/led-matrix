#include "AudioReactiveScenes.h"

#include <algorithm>
#include <cmath>
#include <shared/matrix/plugin/main.h>
#include <shared/matrix/utils/color.h>

namespace Scenes {
namespace {
constexpr float Pi = 3.14159265358979323846f;
float feature(const AudioState::Snapshot &audio, AudioProtocol::Feature id, float gain = 1.0f) {
    return std::clamp(audio.feature(id) * gain, 0.0f, 1.0f);
}
void hsv(float hue, float saturation, float value, uint8_t &r, uint8_t &g, uint8_t &b) {
    color::hsv_to_rgb(hue, saturation, std::clamp(value, 0.0f, 1.0f), r, g, b);
}
void addPixel(rgb_matrix::FrameCanvas *canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= canvas->width() || y >= canvas->height()) return;
    uint8_t oldR = 0, oldG = 0, oldB = 0;
    canvas->GetPixel(x, y, &oldR, &oldG, &oldB);
    canvas->SetPixel(x, y, std::min(255, static_cast<int>(oldR) + r),
                      std::min(255, static_cast<int>(oldG) + g),
                      std::min(255, static_cast<int>(oldB) + b));
}
float wrapped(float value) {
    value -= std::floor(value);
    return std::min(value, 1.0f - value);
}
float tempoTrust(const AudioState::Snapshot &audio) {
    const float confidence = feature(audio, AudioProtocol::Feature::BeatConfidence);
    const float stability = feature(audio, AudioProtocol::Feature::TempoStability);
    return std::clamp((confidence - 0.28f) / 0.52f, 0.0f, 1.0f) * stability;
}
float tempoRate(const AudioState::Snapshot &audio) {
    const float bpm = audio.feature(AudioProtocol::Feature::Bpm);
    if (bpm < 40.0f) return 1.0f;
    const float target = std::clamp(bpm / 120.0f, 0.55f, 1.65f);
    return 1.0f + (target - 1.0f) * tempoTrust(audio);
}
bool consumeEvent(uint64_t current, uint64_t &seen, bool packetFlag) {
    const bool advanced = seen != 0 && current > seen;
    seen = current;
    return packetFlag || advanced;
}
}

void AudioParticleFieldScene::register_properties() {
    add_property(sensitivity_); add_property(particleLimit_); add_property(persistence_);
    add_property(gravity_); add_property(rainbow_); add_property(baseColor_);
    add_property(percussionBursts_); add_property(dropExplosion_);
}

void AudioParticleFieldScene::spawn(const AudioState::Snapshot &audio, int count,
                                    bool radial, float strength) {
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float balance = audio.feature(AudioProtocol::Feature::StereoBalance);
    const float width = feature(audio, AudioProtocol::Feature::StereoWidth);
    const float centroid = feature(audio, AudioProtocol::Feature::SpectralCentroid);
    for (int i = 0; i < count && static_cast<int>(particles_.size()) < particleLimit_->get(); ++i) {
        Particle p{};
        p.x = std::clamp(matrix_width * (0.5f + balance * 0.28f + (unit(rng_) - 0.5f) * (0.18f + width * 0.72f)),
                         0.0f, static_cast<float>(matrix_width - 1));
        p.y = radial ? matrix_height * (0.46f + (unit(rng_) - 0.5f) * 0.12f)
                     : static_cast<float>(matrix_height - 1);
        const float angle = radial ? unit(rng_) * 2.0f * Pi
                                   : -Pi * (0.18f + unit(rng_) * 0.64f);
        const float speed = (18.0f + unit(rng_) * 55.0f) * (0.45f + strength * 1.55f);
        p.vx = std::cos(angle) * speed + balance * 18.0f;
        p.vy = std::sin(angle) * speed;
        p.life = p.maxLife = persistence_->get() * (0.45f + unit(rng_) * 1.25f);
        p.hue = centroid * 180.0f + unit(rng_) * 110.0f;
        p.size = 1.0f + strength * 2.2f + unit(rng_);
        particles_.push_back(p);
    }
}

bool AudioParticleFieldScene::render(rgb_matrix::FrameCanvas *canvas) {
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.05f);
    const float sceneTime = static_cast<float>(frame_context().elapsed_seconds);
    const auto audio = AudioState::snapshot();
    canvas->Clear();
    if (!audio.fresh()) return false;

    const float gain = sensitivity_->get();
    const float bass = feature(audio, AudioProtocol::Feature::Bass, gain);
    const float kick = feature(audio, AudioProtocol::Feature::Kick, gain);
    const float snare = feature(audio, AudioProtocol::Feature::Snare, gain);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat, gain);
    const float loudness = feature(audio, AudioProtocol::Feature::LoudnessFast, gain);
    hueTime_ += dt;

    if (percussionBursts_->get() && consumeEvent(audio.beat_counter, beatSeen_, audio.event(AudioProtocol::BeatEvent))) {
        spawn(audio, 25 + static_cast<int>(kick * 120.0f), true, std::max(kick, 0.35f));
    }
    if (percussionBursts_->get() && consumeEvent(audio.onset_counter, onsetSeen_, audio.event(AudioProtocol::OnsetEvent))) {
        spawn(audio, 5 + static_cast<int>((snare + hihat) * 35.0f), snare > hihat, std::max(snare, hihat));
    }
    if (dropExplosion_->get() && consumeEvent(audio.drop_counter, dropSeen_, audio.event(AudioProtocol::DropEvent))) {
        spawn(audio, std::min(600, particleLimit_->get()), true, 1.0f);
    }

    spawnAccumulator_ += dt * (6.0f + loudness * 80.0f + hihat * 75.0f);
    while (spawnAccumulator_ >= 1.0f) {
        spawn(audio, 1, false, 0.15f + bass * 0.65f + hihat * 0.30f);
        spawnAccumulator_ -= 1.0f;
    }

    const float sideFlow = audio.feature(AudioProtocol::Feature::StereoBalance) * 28.0f;
    const float midFlow = feature(audio, AudioProtocol::Feature::Mid) * 15.0f;
    for (auto &p : particles_) {
        p.life -= dt;
        p.vy += gravity_->get() * dt;
        p.vx += (std::sin(hueTime_ * 1.7f + p.y * 0.07f) * midFlow + sideFlow) * dt;
        p.x += p.vx * dt; p.y += p.vy * dt;
        if (p.x < 0.0f) p.x += matrix_width;
        if (p.x >= matrix_width) p.x -= matrix_width;
        const float life = std::clamp(p.life / p.maxLife, 0.0f, 1.0f);
        uint8_t r, g, b;
        if (rainbow_->get()) hsv(p.hue + hueTime_ * 24.0f, 0.78f, life, r, g, b);
        else { const auto c = baseColor_->get(); r = c.r * life; g = c.g * life; b = c.b * life; }
        const int x = static_cast<int>(std::round(p.x));
        const int y = static_cast<int>(std::round(p.y));
        addPixel(canvas, x, y, r, g, b);
        if (p.size > 1.4f) {
            addPixel(canvas, x + 1, y, r / 2, g / 2, b / 2);
            addPixel(canvas, x - 1, y, r / 2, g / 2, b / 2);
            addPixel(canvas, x, y + 1, r / 2, g / 2, b / 2);
        }
    }
    std::erase_if(particles_, [&](const Particle &p) {
        return p.life <= 0.0f || p.y > matrix_height + 8.0f || p.y < -matrix_height;
    });
    return true;
}

void AudioPulseTunnelScene::register_properties() {
    add_property(sensitivity_); add_property(speed_); add_property(ringCount_);
    add_property(twist_); add_property(rainbow_); add_property(baseColor_);
    add_property(spectrumRibs_); add_property(tempoLock_);
}

bool AudioPulseTunnelScene::render(rgb_matrix::FrameCanvas *canvas) {
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.05f);
    const float sceneTime = static_cast<float>(frame_context().elapsed_seconds);
    const auto audio = AudioState::snapshot();
    canvas->Clear();
    if (!audio.fresh()) return false;
    const float gain = sensitivity_->get();
    const float bass = feature(audio, AudioProtocol::Feature::Bass, gain);
    const float kick = feature(audio, AudioProtocol::Feature::Kick, gain);
    const float snare = feature(audio, AudioProtocol::Feature::Snare, gain);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat, gain);
    const float trust = tempoTrust(audio);
    const float phase = trust > 0.15f ? audio.feature(AudioProtocol::Feature::BeatPhase) : travel_;

    if (consumeEvent(audio.beat_counter, beatSeen_, audio.event(AudioProtocol::BeatEvent))) beatPulse_ = 1.0f;
    if (consumeEvent(audio.drop_counter, dropSeen_, audio.event(AudioProtocol::DropEvent))) dropPulse_ = 1.0f;
    if (consumeEvent(audio.section_counter, sectionSeen_, audio.event(AudioProtocol::SectionEvent))) paletteOffset_ += 73.0f;
    beatPulse_ = std::max(0.0f, beatPulse_ - dt * 4.0f);
    dropPulse_ = std::max(0.0f, dropPulse_ - dt * 1.35f);

    const float tempoSpeed = tempoLock_->get() ? tempoRate(audio) : 1.0f;
    const float kickSurge = kick * (0.45f + trust * 0.35f);
    travel_ = std::fmod(travel_ + dt * speed_->get() * tempoSpeed *
                        (0.42f + bass * 1.15f + kickSurge), 1.0f);
    rotation_ += dt * twist_->get() * (0.12f + snare * 1.25f);
    const float balance = audio.feature(AudioProtocol::Feature::StereoBalance);
    const float width = feature(audio, AudioProtocol::Feature::StereoWidth);
    const float cx = matrix_width * (0.5f + balance * 0.15f);
    const float cy = matrix_height * 0.5f;
    const float maxRadius = std::hypot(matrix_width, matrix_height) * 0.72f;

    for (int y = 0; y < matrix_height; ++y) for (int x = 0; x < matrix_width; ++x) {
        const float dx = x - cx, dy = y - cy;
        const float radius = std::sqrt(dx * dx + dy * dy);
        const float angle = std::atan2(dy, dx) + rotation_;
        const float normalized = radius / maxRadius;
        const float deformation = std::sin(angle * (4.0f + width * 4.0f) + sceneTime * 0.8f) * snare * 0.035f;
        const float ringPhase = std::fmod((normalized + deformation) * ringCount_->get() -
                                         travel_ * ringCount_->get() + 100.0f, 1.0f);
        const float beatWave = std::exp(-std::pow((ringPhase - phase) / (0.055f + dropPulse_ * 0.08f), 2.0f));
        const float line = std::exp(-std::pow((ringPhase - 0.5f) / (0.045f + beatPulse_ * 0.025f), 2.0f));
        float ribs = 0.0f;
        if (spectrumRibs_->get()) {
            const float ribCount = 6.0f + hihat * 18.0f;
            ribs = std::pow(std::max(0.0f, 1.0f - std::abs(std::sin(angle * ribCount))), 12.0f) *
                   (0.08f + hihat * 0.82f);
        }
        const float value = std::clamp((line * (0.35f + bass * 0.75f) + beatWave * 0.55f + ribs +
                                         dropPulse_ * std::max(0.0f, 1.0f - normalized) * 0.45f) *
                                        (0.3f + normalized), 0.0f, 1.0f);
        if (value < 0.02f) continue;
        uint8_t r, g, b;
        if (rainbow_->get()) hsv(paletteOffset_ + angle * 57.3f + normalized * 230.0f + sceneTime * 20.0f,
                                  0.84f, value, r, g, b);
        else { const auto c = baseColor_->get(); r = c.r * value; g = c.g * value; b = c.b * value; }
        canvas->SetPixel(x, y, r, g, b);
    }
    return true;
}

void AudioAuroraScene::register_properties() {
    add_property(ribbonCount_); add_property(flowSpeed_); add_property(sensitivity_);
    add_property(glow_); add_property(stars_);
}

bool AudioAuroraScene::render(rgb_matrix::FrameCanvas *canvas) {
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.05f);
    const float sceneTime = static_cast<float>(frame_context().elapsed_seconds);
    const auto audio = AudioState::snapshot();
    canvas->Clear();
    if (!audio.fresh()) return false;
    time_ += dt * flowSpeed_->get();
    if (consumeEvent(audio.beat_counter, beatSeen_, audio.event(AudioProtocol::BeatEvent))) beatGlow_ = 1.0f;
    if (consumeEvent(audio.drop_counter, dropSeen_, audio.event(AudioProtocol::DropEvent))) dropGlow_ = 1.0f;
    if (consumeEvent(audio.section_counter, sectionSeen_, audio.event(AudioProtocol::SectionEvent))) palette_ += 61.0f;
    beatGlow_ = std::max(0.0f, beatGlow_ - dt * 3.6f);
    dropGlow_ = std::max(0.0f, dropGlow_ - dt * 0.85f);

    const float gain = sensitivity_->get();
    const float bass = feature(audio, AudioProtocol::Feature::Bass, gain);
    const float mid = feature(audio, AudioProtocol::Feature::Mid, gain);
    const float treble = feature(audio, AudioProtocol::Feature::Treble, gain);
    const float width = feature(audio, AudioProtocol::Feature::StereoWidth);
    const float balance = audio.feature(AudioProtocol::Feature::StereoBalance);
    const float correlation = std::clamp(audio.feature(AudioProtocol::Feature::StereoCorrelation), -1.0f, 1.0f);
    const float phaseBreath = tempoTrust(audio) * (0.5f + 0.5f *
        std::cos(audio.feature(AudioProtocol::Feature::BeatPhase) * 2.0f * Pi));
    const int ribbons = ribbonCount_->get();

    for (int y = 0; y < matrix_height; ++y) for (int x = 0; x < matrix_width; ++x) {
        const float nx = static_cast<float>(x) / std::max(1, matrix_width - 1);
        const float ny = static_cast<float>(y) / std::max(1, matrix_height - 1);
        float value = 0.0f;
        float hueMix = 0.0f;
        for (int ribbon = 0; ribbon < ribbons; ++ribbon) {
            const float offset = static_cast<float>(ribbon) / ribbons;
            const float center = 0.18f + offset * 0.64f + balance * 0.08f +
                std::sin(nx * (4.0f + mid * 3.0f) + time_ * (0.7f + offset) + ribbon * 1.7f) *
                (0.035f + bass * 0.11f + width * 0.045f);
            const float thickness = 0.010f + treble * 0.016f + beatGlow_ * 0.010f +
                                    phaseBreath * 0.006f + (1.0f - std::max(0.0f, correlation)) * 0.004f;
            const float distance = std::abs(ny - center);
            const float ribbonValue = std::exp(-distance * distance / std::max(0.0001f, thickness * thickness));
            value += ribbonValue * (0.18f + bass * 0.25f + glow_->get() * 0.22f);
            hueMix += ribbonValue * offset;
        }
        const float verticalGlow = std::max(0.0f, 1.0f - ny) * dropGlow_ * 0.22f;
        value = std::clamp(value + verticalGlow, 0.0f, 1.0f);
        if (value < 0.015f) continue;
        uint8_t r, g, b;
        hsv(palette_ + hueMix * 250.0f + nx * 45.0f + time_ * 8.0f,
            0.72f + treble * 0.2f, value, r, g, b);
        canvas->SetPixel(x, y, r, g, b);
    }

    if (stars_->get() && treble > 0.08f) {
        const int count = static_cast<int>(treble * 55.0f);
        for (int i = 0; i < count; ++i) {
            const uint32_t hash = static_cast<uint32_t>(i * 2654435761U + audio.sequence * 97U);
            const int x = hash % std::max(1, matrix_width);
            const int y = (hash >> 12U) % std::max(1, matrix_height);
            const float sparkle = treble * (0.35f + 0.65f * ((hash >> 24U) / 255.0f));
            addPixel(canvas, x, y, 180 * sparkle, 220 * sparkle, 255 * sparkle);
        }
    }
    return true;
}

void AudioKaleidoscopeScene::register_properties() {
    add_property(symmetry_); add_property(sensitivity_); add_property(rotationSpeed_);
    add_property(detail_); add_property(waveformCore_);
}

bool AudioKaleidoscopeScene::render(rgb_matrix::FrameCanvas *canvas) {
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.05f);
    const float sceneTime = static_cast<float>(frame_context().elapsed_seconds);
    const auto audio = AudioState::snapshot();
    canvas->Clear();
    if (!audio.fresh() || audio.spectrum.empty()) return false;
    if (consumeEvent(audio.beat_counter, beatSeen_, audio.event(AudioProtocol::BeatEvent))) beatPulse_ = 1.0f;
    if (consumeEvent(audio.onset_counter, onsetSeen_, audio.event(AudioProtocol::OnsetEvent))) onsetPulse_ = 1.0f;
    if (consumeEvent(audio.section_counter, sectionSeen_, audio.event(AudioProtocol::SectionEvent))) palette_ += 79.0f;
    beatPulse_ = std::max(0.0f, beatPulse_ - dt * 4.2f);
    onsetPulse_ = std::max(0.0f, onsetPulse_ - dt * 8.0f);
    rotation_ += dt * rotationSpeed_->get() * (0.72f + tempoRate(audio) * 0.55f +
        feature(audio, AudioProtocol::Feature::Snare) * 0.32f);

    const float cx = matrix_width * (0.5f + audio.feature(AudioProtocol::Feature::StereoBalance) * 0.08f);
    const float cy = matrix_height * 0.5f;
    const float maxRadius = std::min(matrix_width, matrix_height) * 0.52f;
    const int symmetry = symmetry_->get();
    const float sector = 2.0f * Pi / symmetry;
    const float gain = sensitivity_->get();
    const float bass = feature(audio, AudioProtocol::Feature::Bass, gain);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat, gain);

    for (int y = 0; y < matrix_height; ++y) for (int x = 0; x < matrix_width; ++x) {
        const float dx = x - cx, dy = y - cy;
        const float radius = std::sqrt(dx * dx + dy * dy);
        if (radius > maxRadius * 1.4f) continue;
        float angle = std::fmod(std::atan2(dy, dx) + rotation_ + 10.0f * Pi, sector);
        if (angle > sector * 0.5f) angle = sector - angle;
        const float radial = radius / maxRadius;
        const float spectrumPosition = std::clamp(radial * 0.72f + angle / (sector * 0.5f) * 0.28f, 0.0f, 1.0f);
        const size_t index = std::min(audio.spectrum.size() - 1,
            static_cast<size_t>(spectrumPosition * (audio.spectrum.size() - 1)));
        const float band = std::clamp(audio.spectrum[index] * gain, 0.0f, 1.0f);
        const float lattice = std::pow(std::max(0.0f,
            std::sin((radial * 16.0f * detail_->get() - band * 7.0f + beatPulse_ * 2.0f) * Pi)),
            5.0f - hihat * 2.5f);
        const float edge = std::pow(std::max(0.0f, 1.0f - angle / (sector * 0.5f)), 8.0f) * onsetPulse_;
        const float value = std::clamp((lattice * (0.12f + band * 0.85f) + edge * 0.55f) *
                                       (1.0f - std::max(0.0f, radial - 1.0f)), 0.0f, 1.0f);
        if (value < 0.02f) continue;
        uint8_t r, g, b;
        hsv(palette_ + spectrumPosition * 290.0f + bass * 60.0f + sceneTime * 10.0f,
            0.86f, value, r, g, b);
        canvas->SetPixel(x, y, r, g, b);
    }

    if (waveformCore_->get() && !audio.waveform.empty()) {
        for (size_t i = 0; i < audio.waveform.size(); ++i) {
            const float angle = static_cast<float>(i) / audio.waveform.size() * 2.0f * Pi + rotation_;
            const float radius = maxRadius * (0.10f + std::abs(audio.waveform[i]) * 0.22f);
            const int x = static_cast<int>(std::round(cx + std::cos(angle) * radius));
            const int y = static_cast<int>(std::round(cy + std::sin(angle) * radius));
            addPixel(canvas, x, y, 220, 240, 255);
        }
    }
    return true;
}

std::unique_ptr<Scenes::Scene> AudioParticleFieldSceneWrapper::create() { return std::make_unique<AudioParticleFieldScene>(); }
std::unique_ptr<Scenes::Scene> AudioPulseTunnelSceneWrapper::create() { return std::make_unique<AudioPulseTunnelScene>(); }
std::unique_ptr<Scenes::Scene> AudioAuroraSceneWrapper::create() { return std::make_unique<AudioAuroraScene>(); }
std::unique_ptr<Scenes::Scene> AudioKaleidoscopeSceneWrapper::create() { return std::make_unique<AudioKaleidoscopeScene>(); }

} // namespace Scenes
