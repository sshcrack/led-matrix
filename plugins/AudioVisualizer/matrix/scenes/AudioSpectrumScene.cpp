#include "AudioSpectrumScene.h"

#include <algorithm>
#include <cmath>
#include <shared/matrix/plugin/main.h>
#include <shared/matrix/utils/color.h>

namespace Scenes {
namespace {
constexpr float Pi = 3.14159265358979323846f;
float tempoTrust(const AudioState::Snapshot &audio) {
    const float confidence = std::clamp(audio.feature(AudioProtocol::Feature::BeatConfidence), 0.0f, 1.0f);
    const float stability = std::clamp(audio.feature(AudioProtocol::Feature::TempoStability), 0.0f, 1.0f);
    return std::clamp((confidence - 0.28f) / 0.52f, 0.0f, 1.0f) * stability;
}
float tempoRate(const AudioState::Snapshot &audio) {
    const float bpm = audio.feature(AudioProtocol::Feature::Bpm);
    if (bpm < 40.0f) return 1.0f;
    const float target = std::clamp(bpm / 120.0f, 0.55f, 1.65f);
    const float trust = tempoTrust(audio);
    return 1.0f + (target - 1.0f) * trust;
}
void addPixel(rgb_matrix::FrameCanvas *canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= canvas->width() || y >= canvas->height()) return;
    uint8_t oldR = 0, oldG = 0, oldB = 0;
    canvas->GetPixel(x, y, &oldR, &oldG, &oldB);
    canvas->SetPixel(x, y, std::min(255, oldR + r), std::min(255, oldG + g), std::min(255, oldB + b));
}
}

void AudioSpectrumScene::register_properties() {
    displayMode_->label("Display mode")
        .description("Choose bars, radial layouts, waveform or scrolling spectrogram.")
        .group("Layout");
    barWidth_->label("Bar width").description("Width of spectrum bars.").group("Bars").unit("px");
    gapWidth_->label("Bar gap").description("Space between adjacent spectrum bars.").group("Bars").unit("px");
    mirror_->label("Mirror bars").description("Layer a horizontally mirrored spectrum behind the primary bars.").group("Bars");
    mirrorLayerBrightness_->label("Mirror backdrop brightness")
        .description("Brightness of the mirrored rear layer. Lower values keep overlapping bars readable with monochrome palettes such as white-on-white.")
        .group("Bars").visible_if("mirror_display", true).step(0.05);
    fallingDots_->label("Peak markers").description("Show falling or outward-moving peak markers.").group("Bars");
    dotFallSpeed_->label("Peak fall speed").description("How quickly peak markers return after a transient.").group("Bars").visible_if("falling_dots", true).step(0.05);
    circleRadius_->label("Radial radius").description("Base radius used by circle and spiral layouts.").group("Radial").step(0.05);
    rotate_->label("Rotate radial layouts").description("Continuously rotate circle and spiral visualizations.").group("Radial");
    rotationSpeed_->label("Rotation speed").description("Speed of radial layout rotation.").group("Radial").visible_if("rotate_visualization", true).step(0.05);
    rainbow_->label("Rainbow palette").description("Color bands by frequency instead of using one base color.").group("Color");
    musicalColor_->label("Musical color shifts").description("Shift hue with spectral centroid, stereo balance and song sections.").group("Color").visible_if("rainbow_colors", true);
    baseColor_->label("Base color").description("Low-frequency/base color when rainbow mode is disabled.").group("Color").visible_if("rainbow_colors", false);
    accentColor_->label("Accent color").description("High-frequency and transient accent color; keeps layered bars readable even with a white base.").group("Color").visible_if("rainbow_colors", false);
    percussionColor_->label("Percussion color accents").description("Let kick, snare and hi-hat subtly shift color emphasis instead of only changing brightness.").group("Color");
    sensitivity_->label("Sensitivity").description("Input gain applied before drawing the spectrum.").group("Response").step(0.05);
    smoothing_->label("Attack smoothing").description("Higher values soften fast upward changes.").group("Response").step(0.02);
    releaseSpeed_->label("Release speed").description("How quickly bars fall after energy disappears.").group("Response").step(0.1);
    beatPulseEnabled_->label("Beat pulse").description("Briefly brighten the visualization on detected beats.").group("Response");
    showWaveform_->label("Waveform overlay").description("Draw the compact waveform over spectrum modes.").group("Overlay");
    waveformStyle_->label("Waveform style").description("Trace, mirrored scope, or filled waveform rendering.").group("Waveform");
    waveformGain_->label("Waveform gain").description("Vertical gain for the waveform without changing spectrum sensitivity.").group("Waveform").step(0.05);
    waveformStabilization_->label("Stabilize waveform")
        .description("Phase-align consecutive audio blocks so the scope morphs smoothly instead of jumping at capture-block boundaries.")
        .group("Waveform");
    waveformSmoothing_->label("Waveform motion smoothing")
        .description("Temporal smoothing applied after phase stabilization. Higher values make the waveform flow more slowly.")
        .group("Waveform").step(0.02);
    waveformThickness_->label("Waveform thickness").description("Trace thickness in pixels. Higher values are brighter but cost slightly more fill work.").group("Waveform");
    stereoMotion_->label("Stereo motion").description("Let stereo balance move radial centers and width open the geometry.").group("Response");

    add_property(barWidth_); add_property(gapWidth_); add_property(mirror_); add_property(mirrorLayerBrightness_);
    add_property(rainbow_); add_property(musicalColor_); add_property(baseColor_); add_property(accentColor_); add_property(percussionColor_);
    add_property(fallingDots_); add_property(dotFallSpeed_); add_property(displayMode_);
    add_property(circleRadius_); add_property(rotate_); add_property(rotationSpeed_);
    add_property(sensitivity_); add_property(smoothing_); add_property(releaseSpeed_);
    add_property(beatPulseEnabled_); add_property(showWaveform_);
    add_property(waveformStyle_); add_property(waveformGain_); add_property(waveformStabilization_); add_property(waveformSmoothing_); add_property(waveformThickness_); add_property(stereoMotion_);
    add_property(useSpotifyArtwork_);
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

void AudioSpectrumScene::updateWaveform(const AudioState::Snapshot &audio, float dt) {
    if (audio.waveform.empty() || matrix_width <= 0) {
        waveformSmoothed_.clear();
        waveformTarget_.clear();
        waveformScratch_.clear();
        return;
    }

    const size_t width = static_cast<size_t>(matrix_width);
    waveformTarget_.resize(width);
    waveformScratch_.resize(width);

    // Resample continuously instead of selecting the strongest signed sample in
    // each display column. Peak-picking makes tiny capture-boundary shifts look
    // like completely different scope shapes.
    const size_t sourceCount = audio.waveform.size();
    for (size_t x = 0; x < width; ++x) {
        const float sourcePosition = width <= 1 || sourceCount <= 1
            ? 0.0f
            : static_cast<float>(x) * static_cast<float>(sourceCount - 1) /
              static_cast<float>(width - 1);
        const size_t lo = std::min(sourceCount - 1, static_cast<size_t>(sourcePosition));
        const size_t hi = std::min(sourceCount - 1, lo + 1);
        const float fraction = sourcePosition - static_cast<float>(lo);
        waveformScratch_[x] = std::clamp(
            audio.waveform[lo] + (audio.waveform[hi] - audio.waveform[lo]) * fraction,
            -1.0f, 1.0f);
    }

    // A small spatial low-pass removes one-pixel saw teeth while retaining the
    // musical shape. It is applied to the target only, so it cannot accumulate
    // and flatten a stationary waveform over time.
    for (size_t x = 0; x < width; ++x) {
        const float left = waveformScratch_[x == 0 ? x : x - 1];
        const float center = waveformScratch_[x];
        const float right = waveformScratch_[x + 1 < width ? x + 1 : x];
        waveformTarget_[x] = left * 0.15f + center * 0.70f + right * 0.15f;
    }

    if (waveformSmoothed_.size() != width) {
        waveformSmoothed_ = waveformTarget_;
        return;
    }

    if (waveformStabilization_->get() && width > 3) {
        float previousEnergy = 0.0f;
        float targetEnergy = 0.0f;
        for (size_t x = 0; x < width; ++x) {
            previousEnergy += waveformSmoothed_[x] * waveformSmoothed_[x];
            targetEnergy += waveformTarget_[x] * waveformTarget_[x];
        }

        if (previousEnergy > 0.0001f && targetEnergy > 0.0001f) {
            size_t bestShift = 0;
            float bestCorrelation = -2.0f;
            const float denominator = std::sqrt(previousEnergy * targetEnergy);
            for (size_t shift = 0; shift < width; ++shift) {
                float dot = 0.0f;
                for (size_t x = 0; x < width; ++x)
                    dot += waveformSmoothed_[x] * waveformTarget_[(x + shift) % width];
                const float correlation = dot / denominator;
                if (correlation > bestCorrelation) {
                    bestCorrelation = correlation;
                    bestShift = shift;
                }
            }

            // Only phase-lock when there is real similarity. Percussive/noisy
            // blocks should morph naturally instead of being forced into an
            // arbitrary cyclic alignment.
            if (bestCorrelation > 0.18f && bestShift != 0) {
                waveformScratch_ = waveformTarget_;
                for (size_t x = 0; x < width; ++x)
                    waveformTarget_[x] = waveformScratch_[(x + bestShift) % width];
            }
        }
    }

    const float retention = std::clamp(waveformSmoothing_->get(), 0.0f, 0.995f);
    const float blend = 1.0f - std::pow(retention, std::max(0.0f, dt) * 60.0f);
    for (size_t x = 0; x < width; ++x)
        waveformSmoothed_[x] += (waveformTarget_[x] - waveformSmoothed_[x]) * blend;
}

void AudioSpectrumScene::colorFor(float position, float intensity,
                                  const AudioState::Snapshot &audio,
                                  uint8_t &r, uint8_t &g, uint8_t &b) const {
    const float kick = std::clamp(audio.feature(AudioProtocol::Feature::Kick), 0.0f, 1.0f);
    const float snare = std::clamp(audio.feature(AudioProtocol::Feature::Snare), 0.0f, 1.0f);
    const float hihat = std::clamp(audio.feature(AudioProtocol::Feature::Hihat), 0.0f, 1.0f);
    const float transientLift = percussionColor_->get()
        ? kick * (1.0f - position) * 0.18f + snare * 0.12f + hihat * position * 0.20f
        : 0.0f;
    intensity = std::clamp(intensity * (1.0f + beatPulse_ * 0.24f) + transientLift, 0.0f, 1.0f);

    if (artworkSnapshot_.valid) {
        const auto c = MediaArtworkState::sample(artworkSnapshot_, position + audio.section_counter * 0.07f);
        r = static_cast<uint8_t>(c.r * intensity);
        g = static_cast<uint8_t>(c.g * intensity);
        b = static_cast<uint8_t>(c.b * intensity);
        return;
    }

    if (!rainbow_->get()) {
        const auto base = baseColor_->get();
        const auto accent = accentColor_->get();
        // The frequency blend deliberately remains visible when the base is
        // white. This gives foreground/high-frequency detail a distinct tint
        // instead of flattening all layered modes into indistinguishable white.
        float mix = std::clamp(0.10f + position * 0.72f, 0.0f, 1.0f);
        if (percussionColor_->get()) mix = std::clamp(mix + snare * 0.10f + hihat * 0.12f, 0.0f, 1.0f);
        r = static_cast<uint8_t>((base.r * (1.0f - mix) + accent.r * mix) * intensity);
        g = static_cast<uint8_t>((base.g * (1.0f - mix) + accent.g * mix) * intensity);
        b = static_cast<uint8_t>((base.b * (1.0f - mix) + accent.b * mix) * intensity);
        return;
    }

    float hue = position * 285.0f;
    if (musicalColor_->get()) {
        hue += audio.feature(AudioProtocol::Feature::SpectralCentroid) * 100.0f;
        hue += audio.feature(AudioProtocol::Feature::StereoBalance) * 24.0f;
        hue += audio.section_counter % 6 * 36.0f;
        if (percussionColor_->get()) hue += kick * -18.0f + snare * 22.0f + hihat * 34.0f;
    }
    const float saturation = std::clamp(0.78f + hihat * 0.14f - kick * 0.05f, 0.55f, 0.96f);
    color::hsv_to_rgb(hue, saturation, intensity, r, g, b);
}

void AudioSpectrumScene::renderBars(rgb_matrix::FrameCanvas *canvas,
                                    const AudioState::Snapshot &audio) {
    const int barWidth = std::max(1, barWidth_->get());
    const int stride = std::max(1, barWidth + gapWidth_->get());
    const int count = std::min<int>(smoothed_.size(),
                                    std::max(1, matrix_width / stride));
    const auto mode = displayMode_->get().get();
    const int centerTop = (matrix_height - 1) / 2;
    const int centerBottom = matrix_height / 2;
    const int centerReach = std::max(1, std::min(centerTop + 1,
                                                 matrix_height - centerBottom));

    auto sourceIndexFor = [&](int displayIndex) -> size_t {
        if (count <= 1 || smoothed_.size() <= 1) return 0;
        return std::min(smoothed_.size() - 1,
            static_cast<size_t>(std::lround(
                static_cast<double>(displayIndex) *
                static_cast<double>(smoothed_.size() - 1) /
                static_cast<double>(count - 1))));
    };

    auto xFor = [&](int displayIndex) {
        if (mode != DisplayMode::EDGES_TO_CENTER)
            return displayIndex * stride;

        // Lay the first half inwards from the left edge and the second half
        // inwards from the right edge. Keep the actual bar flush to the edge;
        // the gap remains on its inward side.
        const int leftCount = (count + 1) / 2;
        if (displayIndex < leftCount)
            return displayIndex * stride;
        return matrix_width - barWidth - (displayIndex - leftCount) * stride;
    };

    auto drawNormalLayerBar = [&](int i, bool mirrored, float brightness) {
        const size_t sourceIndex = sourceIndexFor(i);
        const float value = std::clamp(smoothed_[sourceIndex], 0.0f, 1.0f);
        const float peak = std::clamp(peaks_[sourceIndex], 0.0f, 1.0f);
        const float colorPosition = count <= 1 ? 0.0f
            : static_cast<float>(i) / static_cast<float>(count - 1);
        const int x = i * stride;
        const int barHeight = std::clamp(
            static_cast<int>(std::lround(value * matrix_height)),
            0, matrix_height);

        for (int row = 0; row < barHeight; ++row) {
            const int y = matrix_height - 1 - row;
            const float intensity = 0.4f + 0.6f *
                static_cast<float>(row + 1) /
                static_cast<float>(std::max(1, barHeight));
            uint8_t r, g, b;
            colorFor(colorPosition, intensity, audio, r, g, b);
            r = static_cast<uint8_t>(static_cast<float>(r) * brightness);
            g = static_cast<uint8_t>(static_cast<float>(g) * brightness);
            b = static_cast<uint8_t>(static_cast<float>(b) * brightness);

            for (int w = 0; w < barWidth; ++w) {
                const int sourceX = x + w;
                const int px = mirrored ? matrix_width - 1 - sourceX : sourceX;
                if (px < 0 || px >= matrix_width) continue;
                canvas->SetPixel(px, y, r, g, b);
            }
        }

        if (fallingDots_->get() && peak > 0.001f) {
            const int peakY = std::clamp(
                matrix_height - 1 - static_cast<int>(std::lround(
                    peak * (matrix_height - 1))),
                0, matrix_height - 1);
            uint8_t r, g, b;
            colorFor(colorPosition, 1.0f, audio, r, g, b);
            r = static_cast<uint8_t>(static_cast<float>(r) * brightness);
            g = static_cast<uint8_t>(static_cast<float>(g) * brightness);
            b = static_cast<uint8_t>(static_cast<float>(b) * brightness);
            for (int w = 0; w < barWidth; ++w) {
                const int sourceX = x + w;
                const int px = mirrored ? matrix_width - 1 - sourceX : sourceX;
                if (px < 0 || px >= matrix_width) continue;
                canvas->SetPixel(px, peakY, r, g, b);
            }
        }
    };

    if (mode == DisplayMode::NORMAL) {
        // Treat mirroring as a real rear layer, not as interleaved writes while
        // each foreground bar is drawn. This gives deterministic z-order and
        // lets monochrome configurations use luminance for separation.
        if (mirror_->get()) {
            const float rearBrightness = std::clamp(mirrorLayerBrightness_->get(), 0.0f, 1.0f);
            for (int i = 0; i < count; ++i)
                drawNormalLayerBar(i, true, rearBrightness);
        }
        for (int i = 0; i < count; ++i)
            drawNormalLayerBar(i, false, 1.0f);
        return;
    }

    for (int i = 0; i < count; ++i) {
        const size_t sourceIndex = sourceIndexFor(i);
        const float value = std::clamp(smoothed_[sourceIndex], 0.0f, 1.0f);
        const float peak = std::clamp(peaks_[sourceIndex], 0.0f, 1.0f);
        const float colorPosition = count <= 1 ? 0.0f
            : static_cast<float>(i) / static_cast<float>(count - 1);
        const int x = xFor(i);

        if (mode == DisplayMode::CENTER_OUT) {
            const int extent = std::clamp(
                static_cast<int>(std::lround(value * centerReach)),
                0, centerReach);

            for (int distance = 0; distance < extent; ++distance) {
                const float intensity = 0.35f + 0.65f *
                    static_cast<float>(distance + 1) /
                    static_cast<float>(std::max(1, extent));
                uint8_t r, g, b;
                colorFor(colorPosition, intensity, audio, r, g, b);

                const int upperY = centerTop - distance;
                const int lowerY = centerBottom + distance;
                for (int w = 0; w < barWidth; ++w) {
                    const int px = x + w;
                    if (px < 0 || px >= matrix_width) continue;
                    if (upperY >= 0) canvas->SetPixel(px, upperY, r, g, b);
                    if (lowerY < matrix_height) canvas->SetPixel(px, lowerY, r, g, b);
                }
            }

            if (fallingDots_->get() && peak > 0.001f) {
                const int peakDistance = std::clamp(
                    static_cast<int>(std::lround(peak * (centerReach - 1))),
                    0, centerReach - 1);
                const int upperY = centerTop - peakDistance;
                const int lowerY = centerBottom + peakDistance;
                uint8_t r, g, b;
                colorFor(colorPosition, 1.0f, audio, r, g, b);
                for (int w = 0; w < barWidth; ++w) {
                    const int px = x + w;
                    if (px < 0 || px >= matrix_width) continue;
                    canvas->SetPixel(px, upperY, r, g, b);
                    canvas->SetPixel(px, lowerY, r, g, b);
                }
            }
            continue;
        }

        // EDGES_TO_CENTER is intentionally a single layer. Mirroring it would
        // duplicate and overlap bars that are already laid out symmetrically.
        const int barHeight = std::clamp(
            static_cast<int>(std::lround(value * matrix_height)),
            0, matrix_height);
        for (int row = 0; row < barHeight; ++row) {
            const int y = matrix_height - 1 - row;
            const float intensity = 0.4f + 0.6f *
                static_cast<float>(row + 1) /
                static_cast<float>(std::max(1, barHeight));
            uint8_t r, g, b;
            colorFor(colorPosition, intensity, audio, r, g, b);
            for (int w = 0; w < barWidth; ++w) {
                const int px = x + w;
                if (px < 0 || px >= matrix_width) continue;
                canvas->SetPixel(px, y, r, g, b);
            }
        }

        if (fallingDots_->get() && peak > 0.001f) {
            const int peakY = std::clamp(
                matrix_height - 1 - static_cast<int>(std::lround(
                    peak * (matrix_height - 1))),
                0, matrix_height - 1);
            uint8_t r, g, b;
            colorFor(colorPosition, 1.0f, audio, r, g, b);
            for (int w = 0; w < barWidth; ++w) {
                const int px = x + w;
                if (px < 0 || px >= matrix_width) continue;
                canvas->SetPixel(px, peakY, r, g, b);
            }
        }
    }
}

void AudioSpectrumScene::renderCircle(rgb_matrix::FrameCanvas *canvas,
                                      const AudioState::Snapshot &audio, bool spiral) {
    const float stereoBalance = stereoMotion_->get() ?
        std::clamp(audio.feature(AudioProtocol::Feature::StereoBalance), -1.0f, 1.0f) : 0.0f;
    const float stereoWidth = stereoMotion_->get() ?
        std::clamp(audio.feature(AudioProtocol::Feature::StereoWidth), 0.0f, 1.0f) : 0.0f;
    const float kick = std::clamp(audio.feature(AudioProtocol::Feature::Kick), 0.0f, 1.0f);
    const float cx = matrix_width * (0.5f + stereoBalance * 0.11f);
    const float cy = matrix_height * 0.5f;
    const float size = static_cast<float>(std::min(matrix_width, matrix_height));
    const float baseRadius = size * 0.21f * circleRadius_->get() * (1.0f + kick * 0.10f);
    const float maxLength = size * 0.30f;

    for (size_t i = 0; i < smoothed_.size(); ++i) {
        const float t = static_cast<float>(i) /
            static_cast<float>(std::max<size_t>(1, smoothed_.size() - 1));
        const float angle = rotation_ + t * 2.0f * Pi *
            (spiral ? 2.4f : 1.0f);
        const float radius = baseRadius *
            (spiral ? 0.35f + t * (1.5f + stereoWidth * 0.28f) : 1.0f + stereoWidth * 0.08f);
        const float length = std::clamp(smoothed_[i], 0.0f, 1.0f) * maxLength;

        for (float distance = 0.0f; distance <= length; distance += 0.7f) {
            const int x = static_cast<int>(std::round(
                cx + std::cos(angle) * (radius + distance)));
            const int y = static_cast<int>(std::round(
                cy + std::sin(angle) * (radius + distance)));
            uint8_t r, g, b;
            colorFor(t, 0.28f + 0.72f * distance /
                std::max(1.0f, length), audio, r, g, b);
            addPixel(canvas, x, y, r, g, b);
        }

        // Falling markers follow the same polar/spiral path as their bar.
        // Previously these modes had no mode-aware peak marker at all.
        if (fallingDots_->get() && i < peaks_.size() && peaks_[i] > 0.001f) {
            const float peakRadius = radius +
                std::clamp(peaks_[i], 0.0f, 1.0f) * maxLength;
            const float tangentX = -std::sin(angle);
            const float tangentY = std::cos(angle);
            uint8_t r, g, b;
            colorFor(t, 1.0f, audio, r, g, b);

            for (int width = -1; width <= 1; ++width) {
                const int x = static_cast<int>(std::round(
                    cx + std::cos(angle) * peakRadius + tangentX * width));
                const int y = static_cast<int>(std::round(
                    cy + std::sin(angle) * peakRadius + tangentY * width));
                addPixel(canvas, x, y, r, g, b);
            }
        }
    }
}

void AudioSpectrumScene::renderWaveform(rgb_matrix::FrameCanvas *canvas,
                                        const AudioState::Snapshot &audio) {
    if (waveformSmoothed_.empty()) return;

    const bool standalone = displayMode_->get().get() == DisplayMode::WAVEFORM;
    const float balance = stereoMotion_->get()
        ? std::clamp(audio.feature(AudioProtocol::Feature::StereoBalance), -1.0f, 1.0f)
        : 0.0f;
    const float loudness = std::clamp(audio.feature(AudioProtocol::Feature::LoudnessFast), 0.0f, 1.0f);
    const float center = matrix_height * (0.5f + balance * 0.035f);
    const float amplitude = matrix_height * (standalone ? 0.44f : 0.31f) * waveformGain_->get()
        * (0.86f + loudness * 0.10f + beatPulse_ * 0.08f);
    const int thickness = std::clamp(waveformThickness_->get(), 1, 3);
    const auto style = waveformStyle_->get().get();

    auto sampleForX = [&](int x) {
        const size_t index = std::min(
            waveformSmoothed_.size() - 1,
            static_cast<size_t>(std::max(0, x)));
        return std::clamp(waveformSmoothed_[index], -1.0f, 1.0f);
    };

    auto drawTracePoint = [&](int x, int y, float position, float intensity) {
        uint8_t r = 0, g = 0, b = 0;
        colorFor(position, std::clamp(intensity, 0.0f, 1.0f), audio, r, g, b);
        for (int offset = -(thickness - 1); offset <= thickness - 1; ++offset) {
            const float falloff = offset == 0 ? 1.0f : 0.42f;
            addPixel(canvas, x, y + offset,
                static_cast<uint8_t>(r * falloff),
                static_cast<uint8_t>(g * falloff),
                static_cast<uint8_t>(b * falloff));
        }
    };

    if (style == WaveformStyle::MIRRORED) {
        int previousUpper = static_cast<int>(std::lround(center));
        int previousLower = previousUpper;
        for (int x = 0; x < matrix_width; ++x) {
            const float sample = std::abs(sampleForX(x));
            const int upper = std::clamp(static_cast<int>(std::lround(center - sample * amplitude)), 0, matrix_height - 1);
            const int lower = std::clamp(static_cast<int>(std::lround(center + sample * amplitude)), 0, matrix_height - 1);
            const float position = static_cast<float>(x) / static_cast<float>(std::max(1, matrix_width - 1));
            for (int y = std::min(previousUpper, upper); y <= std::max(previousUpper, upper); ++y)
                drawTracePoint(x, y, position, standalone ? 0.95f : 0.68f);
            for (int y = std::min(previousLower, lower); y <= std::max(previousLower, lower); ++y)
                drawTracePoint(x, y, position, standalone ? 0.95f : 0.68f);
            previousUpper = upper;
            previousLower = lower;
        }
        return;
    }

    int previousY = static_cast<int>(std::lround(center));
    for (int x = 0; x < matrix_width; ++x) {
        const float sample = sampleForX(x);
        const int y = std::clamp(static_cast<int>(std::lround(center - sample * amplitude)), 0, matrix_height - 1);
        const float position = static_cast<float>(x) / static_cast<float>(std::max(1, matrix_width - 1));

        if (style == WaveformStyle::FILLED) {
            const int mid = std::clamp(static_cast<int>(std::lround(center)), 0, matrix_height - 1);
            const int from = std::min(mid, y), to = std::max(mid, y);
            const int span = std::max(1, to - from);
            for (int py = from; py <= to; ++py) {
                const float edge = std::abs(static_cast<float>(py - mid)) / static_cast<float>(span);
                uint8_t r = 0, g = 0, b = 0;
                colorFor(position, (standalone ? 0.26f : 0.15f) + edge * (standalone ? 0.62f : 0.40f), audio, r, g, b);
                addPixel(canvas, x, py, r, g, b);
            }
            drawTracePoint(x, y, position, standalone ? 1.0f : 0.72f);
        } else {
            const int from = std::min(previousY, y), to = std::max(previousY, y);
            for (int py = from; py <= to; ++py)
                drawTracePoint(x, py, position, standalone ? 0.96f : 0.68f);
        }
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
    const float dt = std::clamp(static_cast<float>(frame_context().delta_seconds), 0.0f, 0.05f);
    const auto audio = AudioState::snapshot();
    artworkSnapshot_ = useSpotifyArtwork_->get() ? MediaArtworkState::snapshot() : MediaArtworkState::Snapshot{};
    canvas->Clear();
    if (!audio.fresh() || audio.spectrum.empty()) return false;

    updateSpectrum(audio, dt);
    const auto mode = displayMode_->get().get();
    if (mode == DisplayMode::WAVEFORM || showWaveform_->get())
        updateWaveform(audio, dt);
    else {
        waveformSmoothed_.clear();
        waveformTarget_.clear();
        waveformScratch_.clear();
    }
    if (beatPulseEnabled_->get()) {
        if (audio.event(AudioProtocol::BeatEvent) || (lastBeat_ != 0 && audio.beat_counter > lastBeat_))
            beatPulse_ = 1.0f;
        beatPulse_ = std::max(0.0f, beatPulse_ - dt * 4.5f);
    } else {
        beatPulse_ = 0.0f;
    }
    lastBeat_ = audio.beat_counter;
    if (rotate_->get()) {
        const float snare = std::clamp(audio.feature(AudioProtocol::Feature::Snare), 0.0f, 1.0f);
        rotation_ += dt * rotationSpeed_->get() * (0.45f + tempoRate(audio) * 0.55f + snare * 0.18f);
    }

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

Scenes::SceneDescriptor Scenes::AudioSpectrumScene::get_descriptor() const {
    auto d = Scene::get_descriptor(); d.automatic_eligible = true;
    d.family = "spectrum";
    d.tags = {"music", "spectrum", "bars", "waveform", "audio-reactive"};
    d.intensity = 0.78f; d.motion = 0.72f; d.music_affinity = 1.0f; d.performance_cost = 0.58f;
    d.variants = {
        {"bars", "Layered bars", "Balanced spectrum bars with mirrored depth and peak motion",
         {{"display_mode", "NORMAL"}, {"mirror_display", true}, {"musical_color", true}, {"falling_dots", true}, {"smoothing", 0.70f}},
         {"music", "bars"}, 0.72f, 0.68f, 1.0f, 0.52f},
        {"waveform", "Smooth waveform", "A clean stabilized trace instead of individual spectrum bars",
         {{"display_mode", "WAVEFORM"}, {"waveform_style", "TRACE"}, {"waveform_stabilization", true}, {"waveform_smoothing", 0.90f}, {"waveform_thickness", 1}, {"stereo_motion", true}},
         {"music", "waveform", "minimal"}, 0.52f, 0.56f, 1.0f, 0.42f},
        {"radial", "Radial spectrum", "Circular spectrum with slow rotation and beat pulse",
         {{"display_mode", "CIRCLE"}, {"circle_radius", 0.70f}, {"rotate_visualization", true}, {"rotation_speed", 0.45f}, {"beat_pulse", true}},
         {"music", "radial"}, 0.80f, 0.78f, 1.0f, 0.64f},
        {"spectrogram", "Spectrogram", "Scrolling frequency history for a denser analytical texture",
         {{"display_mode", "SPECTROGRAM"}, {"mirror_display", false}, {"falling_dots", false}, {"musical_color", true}},
         {"music", "texture", "dense"}, 0.68f, 0.60f, 1.0f, 0.72f},
    };
    return d;
}
