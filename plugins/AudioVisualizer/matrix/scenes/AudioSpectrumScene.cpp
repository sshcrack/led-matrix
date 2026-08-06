#include "AudioSpectrumScene.h"

#include <algorithm>
#include <cmath>
#include <shared/matrix/plugin/main.h>
#include <shared/matrix/utils/color.h>

namespace Scenes {
namespace {
constexpr float Pi = 3.14159265358979323846f;
void addPixel(rgb_matrix::FrameCanvas *canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= canvas->width() || y >= canvas->height()) return;
    uint8_t oldR = 0, oldG = 0, oldB = 0;
    canvas->GetPixel(x, y, &oldR, &oldG, &oldB);
    canvas->SetPixel(x, y, std::min(255, oldR + r), std::min(255, oldG + g), std::min(255, oldB + b));
}
}

void AudioSpectrumScene::register_properties() {
    add_property(barWidth_); add_property(gapWidth_); add_property(mirror_);
    add_property(rainbow_); add_property(musicalColor_); add_property(baseColor_);
    add_property(fallingDots_); add_property(dotFallSpeed_); add_property(displayMode_);
    add_property(circleRadius_); add_property(rotate_); add_property(rotationSpeed_);
    add_property(sensitivity_); add_property(smoothing_); add_property(releaseSpeed_);
    add_property(beatPulseEnabled_); add_property(showWaveform_);
}

void AudioSpectrumScene::updateSpectrum(const AudioState::Snapshot &audio, float dt) {
    if (smoothed_.size() != audio.spectrum.size()) {
        smoothed_.assign(audio.spectrum.size(), 0.0f);
        peaks_.assign(audio.spectrum.size(), 0.0f);
        history_.clear();
    }
    const float attack = 1.0f - std::pow(std::clamp(smoothing_->get(), 0.0f, 0.99f), dt * 60.0f);
    for (size_t i = 0; i < audio.spectrum.size(); ++i) {
        const float target = std::clamp(audio.spectrum[i] * sensitivity_->get(), 0.0f, 1.0f);
        if (target > smoothed_[i]) smoothed_[i] += (target - smoothed_[i]) * attack;
        else smoothed_[i] = std::max(target, smoothed_[i] - releaseSpeed_->get() * dt * 0.16f);
        peaks_[i] = target > peaks_[i] ? target : std::max(0.0f, peaks_[i] - dotFallSpeed_->get() * dt);
    }
    history_.push_front(smoothed_);
    while (history_.size() > static_cast<size_t>(std::max(matrix_width, matrix_height))) history_.pop_back();
}

void AudioSpectrumScene::colorFor(float position, float intensity,
                                  const AudioState::Snapshot &audio,
                                  uint8_t &r, uint8_t &g, uint8_t &b) const {
    intensity = std::clamp(intensity * (1.0f + beatPulse_ * 0.28f), 0.0f, 1.0f);
    if (!rainbow_->get()) {
        const auto base = baseColor_->get();
        r = static_cast<uint8_t>(base.r * intensity);
        g = static_cast<uint8_t>(base.g * intensity);
        b = static_cast<uint8_t>(base.b * intensity);
        return;
    }
    float hue = position * 285.0f;
    if (musicalColor_->get()) {
        hue += audio.feature(AudioProtocol::Feature::SpectralCentroid) * 100.0f;
        hue += audio.feature(AudioProtocol::Feature::StereoBalance) * 24.0f;
        hue += audio.section_counter % 6 * 36.0f;
    }
    color::hsv_to_rgb(hue, 0.84f, intensity, r, g, b);
}

void AudioSpectrumScene::renderBars(rgb_matrix::FrameCanvas *canvas,
                                    const AudioState::Snapshot &audio) {
    const int stride = barWidth_->get() + gapWidth_->get();
    const int count = std::min<int>(smoothed_.size(), std::max(1, matrix_width / std::max(1, stride)));
    for (int i = 0; i < count; ++i) {
        int x = i * stride;
        if (displayMode_->get().get() == DisplayMode::EDGES_TO_CENTER) {
            const int half = count / 2;
            x = i < half ? i * stride : matrix_width - (i - half + 1) * stride;
        }
        const float value = smoothed_[i];
        const int barHeight = std::clamp(static_cast<int>(value * matrix_height), 0, matrix_height);
        for (int w = 0; w < barWidth_->get(); ++w) {
            if (displayMode_->get().get() == DisplayMode::CENTER_OUT) {
                const int halfHeight = static_cast<int>(value * matrix_height * 0.5f);
                for (int dy = -halfHeight; dy <= halfHeight; ++dy) {
                    uint8_t r, g, b; colorFor(static_cast<float>(i) / count,
                        0.35f + 0.65f * std::abs(dy) / std::max(1, halfHeight), audio, r, g, b);
                    canvas->SetPixel(x + w, matrix_height / 2 + dy, r, g, b);
                }
            } else {
                for (int y = matrix_height - 1; y >= matrix_height - barHeight; --y) {
                    uint8_t r, g, b; colorFor(static_cast<float>(i) / count,
                        0.4f + 0.6f * static_cast<float>(matrix_height - y) / matrix_height,
                        audio, r, g, b);
                    canvas->SetPixel(x + w, y, r, g, b);
                    if (mirror_->get()) canvas->SetPixel(matrix_width - 1 - x - w, y, r, g, b);
                }
            }
        }
        if (fallingDots_->get()) {
            const int y = matrix_height - 1 - static_cast<int>(peaks_[i] * (matrix_height - 1));
            uint8_t r, g, b; colorFor(static_cast<float>(i) / count, 1.0f, audio, r, g, b);
            canvas->SetPixel(x, y, r, g, b);
        }
    }
}

void AudioSpectrumScene::renderCircle(rgb_matrix::FrameCanvas *canvas,
                                      const AudioState::Snapshot &audio, bool spiral) {
    const float cx = matrix_width * 0.5f;
    const float cy = matrix_height * 0.5f;
    const float baseRadius = std::min(matrix_width, matrix_height) * 0.21f * circleRadius_->get();
    for (size_t i = 0; i < smoothed_.size(); ++i) {
        const float t = static_cast<float>(i) / std::max<size_t>(1, smoothed_.size());
        const float angle = rotation_ + t * 2.0f * Pi * (spiral ? 2.4f : 1.0f);
        const float radius = baseRadius * (spiral ? 0.35f + t * 1.5f : 1.0f);
        const float length = smoothed_[i] * std::min(matrix_width, matrix_height) * 0.30f;
        for (float d = 0.0f; d <= length; d += 0.7f) {
            const int x = static_cast<int>(std::round(cx + std::cos(angle) * (radius + d)));
            const int y = static_cast<int>(std::round(cy + std::sin(angle) * (radius + d)));
            uint8_t r, g, b; colorFor(t, 0.28f + 0.72f * d / std::max(1.0f, length), audio, r, g, b);
            addPixel(canvas, x, y, r, g, b);
        }
    }
}

void AudioSpectrumScene::renderWaveform(rgb_matrix::FrameCanvas *canvas,
                                        const AudioState::Snapshot &audio) {
    if (audio.waveform.empty()) return;
    int previousY = matrix_height / 2;
    for (int x = 0; x < matrix_width; ++x) {
        const size_t index = std::min(audio.waveform.size() - 1,
            static_cast<size_t>(static_cast<float>(x) / std::max(1, matrix_width - 1) * (audio.waveform.size() - 1)));
        const int y = static_cast<int>(matrix_height * 0.5f - audio.waveform[index] * matrix_height * 0.42f);
        const int from = std::min(previousY, y), to = std::max(previousY, y);
        uint8_t r, g, b; colorFor(static_cast<float>(x) / matrix_width, 0.9f, audio, r, g, b);
        for (int py = from; py <= to; ++py) addPixel(canvas, x, py, r, g, b);
        previousY = y;
    }
}

void AudioSpectrumScene::renderSpectrogram(rgb_matrix::FrameCanvas *canvas,
                                           const AudioState::Snapshot &audio) {
    for (int x = 0; x < matrix_width && x < static_cast<int>(history_.size()); ++x) {
        const auto &column = history_[x];
        for (int y = 0; y < matrix_height; ++y) {
            const size_t index = std::min(column.size() - 1,
                static_cast<size_t>((1.0f - static_cast<float>(y) / matrix_height) * (column.size() - 1)));
            const float value = std::clamp(column[index] * (1.0f - x / static_cast<float>(matrix_width) * 0.45f), 0.0f, 1.0f);
            if (value < 0.025f) continue;
            uint8_t r, g, b; colorFor(static_cast<float>(index) / column.size(), value, audio, r, g, b);
            canvas->SetPixel(matrix_width - 1 - x, y, r, g, b);
        }
    }
}

bool AudioSpectrumScene::render(rgb_matrix::FrameCanvas *canvas) {
    const auto tick = timer_.tick();
    const float dt = std::clamp(static_cast<float>(tick.deltaFrame.count()), 0.0f, 0.05f);
    const auto audio = AudioState::snapshot();
    canvas->Clear();
    if (!audio.fresh() || audio.spectrum.empty()) return false;

    updateSpectrum(audio, dt);
    if (audio.event(AudioProtocol::BeatEvent) || (lastBeat_ != 0 && audio.beat_counter > lastBeat_)) beatPulse_ = 1.0f;
    lastBeat_ = audio.beat_counter;
    beatPulse_ = std::max(0.0f, beatPulse_ - dt * 4.5f);
    if (rotate_->get()) rotation_ += dt * rotationSpeed_->get() *
        (0.35f + audio.feature(AudioProtocol::Feature::Bpm) / 140.0f);

    switch (displayMode_->get().get()) {
    case DisplayMode::CIRCLE: renderCircle(canvas, audio, false); break;
    case DisplayMode::SPIRAL: renderCircle(canvas, audio, true); break;
    case DisplayMode::WAVEFORM: renderWaveform(canvas, audio); break;
    case DisplayMode::SPECTROGRAM: renderSpectrogram(canvas, audio); break;
    default: renderBars(canvas, audio); break;
    }
    if (showWaveform_->get() && displayMode_->get().get() != DisplayMode::WAVEFORM)
        renderWaveform(canvas, audio);
    return true;
}

std::unique_ptr<Scenes::Scene> AudioSpectrumSceneWrapper::create() {
    return std::make_unique<AudioSpectrumScene>();
}
}
