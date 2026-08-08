#include "BasicEffects.h"
#include "shared/common/plugin_macros.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace Plugins;

namespace {
constexpr float Pi = 3.14159265358979323846f;

float decay_envelope(const PostProcessEffect &effect) {
    const float progress = std::clamp(PostProcessingEffect::get_effect_progress(effect), 0.0f, 1.0f);
    const float remaining = 1.0f - progress;
    return std::max(0.0f, effect.intensity) * remaining * remaining;
}

void capture(FrameCanvas *canvas, std::vector<rgb_matrix::Color> &scratch) {
    const int width = canvas->width();
    const int height = canvas->height();
    scratch.resize(static_cast<size_t>(width * height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t r = 0, g = 0, b = 0;
            canvas->GetPixel(x, y, &r, &g, &b);
            scratch[static_cast<size_t>(y * width + x)] = rgb_matrix::Color(r, g, b);
        }
    }
}

const rgb_matrix::Color &sample(const std::vector<rgb_matrix::Color> &scratch,
                                int width, int height, int x, int y) {
    x = std::clamp(x, 0, width - 1);
    y = std::clamp(y, 0, height - 1);
    return scratch[static_cast<size_t>(y * width + x)];
}

uint8_t mix_channel(uint8_t a, uint8_t b, float t) {
    return static_cast<uint8_t>(std::clamp(
        static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t,
        0.0f, 255.0f));
}

uint32_t hash32(uint32_t v) {
    v ^= v >> 16;
    v *= 0x7feb352dU;
    v ^= v >> 15;
    v *= 0x846ca68bU;
    v ^= v >> 16;
    return v;
}
}

REGISTER_PLUGIN(BasicEffects, BasicEffects)

BasicEffects::BasicEffects() = default;

vector<std::unique_ptr<ImageProviderWrapper>> BasicEffects::create_image_providers() { return {}; }
vector<std::unique_ptr<SceneWrapper>> BasicEffects::create_scenes() { return {}; }

vector<std::unique_ptr<PostProcessingEffect>> BasicEffects::create_effects() {
    vector<std::unique_ptr<PostProcessingEffect>> effects;
    effects.push_back(std::make_unique<FlashEffect>());
    effects.push_back(std::make_unique<RotateEffect>());
    effects.push_back(std::make_unique<GlowEffect>());
    effects.push_back(std::make_unique<RgbSplitEffect>());
    effects.push_back(std::make_unique<GlitchEffect>());
    effects.push_back(std::make_unique<PixelateEffect>());
    effects.push_back(std::make_unique<ShockwaveEffect>());
    return effects;
}

std::string BasicEffects::get_plugin_name() const { return "BasicEffects"; }

void FlashEffect::apply(FrameCanvas *canvas, const PostProcessEffect &effect) {
    if (!canvas) return;
    const float progress = get_effect_progress(effect);
    float flash_intensity = 0.0f;
    if (progress < 0.1f)
        flash_intensity = (progress / 0.1f) * effect.intensity;
    else
        flash_intensity = effect.intensity * std::exp(-((progress - 0.1f) / 0.9f) * 5.0f);

    for (int y = 0; y < canvas->height(); ++y) {
        for (int x = 0; x < canvas->width(); ++x) {
            uint8_t r = 0, g = 0, b = 0;
            canvas->GetPixel(x, y, &r, &g, &b);
            if (r == 0 && g == 0 && b == 0) continue;
            canvas->SetPixel(
                x, y,
                std::min(255, static_cast<int>(r + flash_intensity * (255 - r) * 0.8f)),
                std::min(255, static_cast<int>(g + flash_intensity * (255 - g) * 0.8f)),
                std::min(255, static_cast<int>(b + flash_intensity * (255 - b) * 0.8f)));
        }
    }
}

void RotateEffect::apply(FrameCanvas *canvas, const PostProcessEffect &effect) {
    if (!canvas) return;
    const float rotation_degrees = get_effect_progress(effect) * 360.0f * effect.intensity;
    if (std::abs(rotation_degrees) < 1.0f) return;

    const int width = canvas->width();
    const int height = canvas->height();
    capture(canvas, scratch_);
    const float radians = rotation_degrees * Pi / 180.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    const float cx = (width - 1) * 0.5f;
    const float cy = (height - 1) * 0.5f;

    canvas->Clear();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float rx = static_cast<float>(x) - cx;
            const float ry = static_cast<float>(y) - cy;
            const int sx = static_cast<int>(std::lround(rx * c + ry * s + cx));
            const int sy = static_cast<int>(std::lround(-rx * s + ry * c + cy));
            if (sx < 0 || sx >= width || sy < 0 || sy >= height) continue;
            const auto &color = scratch_[static_cast<size_t>(sy * width + sx)];
            canvas->SetPixel(x, y, color.r, color.g, color.b);
        }
    }
}

void GlowEffect::apply(FrameCanvas *canvas, const PostProcessEffect &effect) {
    if (!canvas) return;
    const float amount = std::clamp(decay_envelope(effect), 0.0f, 1.5f);
    if (amount < 0.01f) return;
    const int width = canvas->width();
    const int height = canvas->height();
    capture(canvas, scratch_);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto &center = sample(scratch_, width, height, x, y);
            const auto &left = sample(scratch_, width, height, x - 1, y);
            const auto &right = sample(scratch_, width, height, x + 1, y);
            const auto &up = sample(scratch_, width, height, x, y - 1);
            const auto &down = sample(scratch_, width, height, x, y + 1);
            const int blur_r = (center.r * 2 + left.r + right.r + up.r + down.r) / 6;
            const int blur_g = (center.g * 2 + left.g + right.g + up.g + down.g) / 6;
            const int blur_b = (center.b * 2 + left.b + right.b + up.b + down.b) / 6;
            const float gain = 0.62f * amount;
            canvas->SetPixel(
                x, y,
                std::min(255, static_cast<int>(center.r + blur_r * gain)),
                std::min(255, static_cast<int>(center.g + blur_g * gain)),
                std::min(255, static_cast<int>(center.b + blur_b * gain)));
        }
    }
}

void RgbSplitEffect::apply(FrameCanvas *canvas, const PostProcessEffect &effect) {
    if (!canvas) return;
    const float amount = std::clamp(decay_envelope(effect), 0.0f, 1.0f);
    if (amount < 0.01f) return;
    const int width = canvas->width();
    const int height = canvas->height();
    capture(canvas, scratch_);
    const int offset = std::max(1, static_cast<int>(std::lround(1.0f + amount * 4.0f)));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto &base = sample(scratch_, width, height, x, y);
            const auto &red = sample(scratch_, width, height, x - offset, y);
            const auto &blue = sample(scratch_, width, height, x + offset, y);
            canvas->SetPixel(x, y,
                mix_channel(base.r, red.r, amount),
                base.g,
                mix_channel(base.b, blue.b, amount));
        }
    }
}

void GlitchEffect::apply(FrameCanvas *canvas, const PostProcessEffect &effect) {
    if (!canvas) return;
    const float amount = std::clamp(decay_envelope(effect), 0.0f, 1.0f);
    if (amount < 0.01f) return;
    const int width = canvas->width();
    const int height = canvas->height();
    capture(canvas, scratch_);
    const uint32_t phase = static_cast<uint32_t>(get_effect_progress(effect) * 127.0f);
    const int max_shift = std::max(1, static_cast<int>(std::lround(amount * 8.0f)));

    for (int y = 0; y < height; ++y) {
        const uint32_t h = hash32(static_cast<uint32_t>(y) ^ (phase * 0x9e3779b9U));
        const bool shifted = (h & 0xffU) < static_cast<uint32_t>(42.0f + amount * 118.0f);
        const int shift = shifted
            ? static_cast<int>(h % static_cast<uint32_t>(max_shift * 2 + 1)) - max_shift
            : 0;
        const int channel_shift = shifted && ((h >> 8) & 1U) ? 1 : 0;
        for (int x = 0; x < width; ++x) {
            const auto &base = sample(scratch_, width, height, x + shift, y);
            const auto &red = sample(scratch_, width, height, x + shift - channel_shift, y);
            const auto &blue = sample(scratch_, width, height, x + shift + channel_shift, y);
            canvas->SetPixel(x, y, red.r, base.g, blue.b);
        }
    }
}

void PixelateEffect::apply(FrameCanvas *canvas, const PostProcessEffect &effect) {
    if (!canvas) return;
    const float amount = std::clamp(decay_envelope(effect), 0.0f, 1.0f);
    const int block = std::max(1, static_cast<int>(std::lround(1.0f + amount * 7.0f)));
    if (block <= 1) return;
    const int width = canvas->width();
    const int height = canvas->height();

    for (int by = 0; by < height; by += block) {
        for (int bx = 0; bx < width; bx += block) {
            uint32_t rs = 0, gs = 0, bs = 0, count = 0;
            const int y_end = std::min(height, by + block);
            const int x_end = std::min(width, bx + block);
            for (int y = by; y < y_end; ++y) {
                for (int x = bx; x < x_end; ++x) {
                    uint8_t r = 0, g = 0, b = 0;
                    canvas->GetPixel(x, y, &r, &g, &b);
                    rs += r; gs += g; bs += b; ++count;
                }
            }
            if (count == 0) continue;
            const uint8_t r = static_cast<uint8_t>(rs / count);
            const uint8_t g = static_cast<uint8_t>(gs / count);
            const uint8_t b = static_cast<uint8_t>(bs / count);
            for (int y = by; y < y_end; ++y)
                for (int x = bx; x < x_end; ++x)
                    canvas->SetPixel(x, y, r, g, b);
        }
    }
}

void ShockwaveEffect::apply(FrameCanvas *canvas, const PostProcessEffect &effect) {
    if (!canvas) return;
    const float progress = std::clamp(get_effect_progress(effect), 0.0f, 1.0f);
    const float amount = std::clamp(effect.intensity, 0.0f, 1.5f) * (1.0f - progress);
    if (amount < 0.01f) return;
    const int width = canvas->width();
    const int height = canvas->height();
    capture(canvas, scratch_);

    const float cx = (width - 1) * 0.5f;
    const float cy = (height - 1) * 0.5f;
    const float max_radius = std::hypot(cx, cy);
    const float radius = progress * max_radius * 1.12f;
    const float band = 2.0f + std::max(0.0f, effect.intensity) * 4.0f;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float delta = std::abs(distance - radius);
            if (delta > band || distance < 0.5f) continue;
            const float wave = 1.0f - delta / band;
            const float displacement = wave * amount * 4.0f;
            const int sx = static_cast<int>(std::lround(x - dx / distance * displacement));
            const int sy = static_cast<int>(std::lround(y - dy / distance * displacement));
            const auto &color = sample(scratch_, width, height, sx, sy);
            const float lift = wave * amount * 0.30f;
            canvas->SetPixel(x, y,
                std::min(255, static_cast<int>(color.r + lift * (255 - color.r))),
                std::min(255, static_cast<int>(color.g + lift * (255 - color.g))),
                std::min(255, static_cast<int>(color.b + lift * (255 - color.b))));
        }
    }
}
