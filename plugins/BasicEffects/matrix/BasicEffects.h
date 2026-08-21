#pragma once

#include "shared/matrix/plugin/main.h"
#include <vector>

namespace Plugins {
    class BasicEffects : public BasicPlugin {
    public:
        BasicEffects();
        ~BasicEffects() override = default;

        vector<std::unique_ptr<ImageProviderWrapper>> create_image_providers() override;
        vector<std::unique_ptr<SceneWrapper>> create_scenes() override;
        std::string get_plugin_name() const override;
        vector<std::unique_ptr<PostProcessingEffect>> create_effects() override;
    };
}

class FlashEffect : public PostProcessingEffect {
public:
    std::string get_name() const override { return "flash"; }
    void apply(FrameCanvas* canvas, const PostProcessEffect& effect) override;
};

class RotateEffect : public PostProcessingEffect {
    std::vector<rgb_matrix::Color> scratch_;
public:
    std::string get_name() const override { return "rotate"; }
    void apply(FrameCanvas* canvas, const PostProcessEffect& effect) override;
};

class GlowEffect : public PostProcessingEffect {
    std::vector<rgb_matrix::Color> scratch_;
public:
    std::string get_name() const override { return "glow"; }
    void apply(FrameCanvas* canvas, const PostProcessEffect& effect) override;
};

class RgbSplitEffect : public PostProcessingEffect {
    std::vector<rgb_matrix::Color> scratch_;
public:
    std::string get_name() const override { return "rgb_split"; }
    void apply(FrameCanvas* canvas, const PostProcessEffect& effect) override;
};

class GlitchEffect : public PostProcessingEffect {
    std::vector<rgb_matrix::Color> scratch_;
public:
    std::string get_name() const override { return "glitch"; }
    void apply(FrameCanvas* canvas, const PostProcessEffect& effect) override;
};

class PixelateEffect : public PostProcessingEffect {
public:
    std::string get_name() const override { return "pixelate"; }
    void apply(FrameCanvas* canvas, const PostProcessEffect& effect) override;
};

class ShockwaveEffect : public PostProcessingEffect {
    std::vector<rgb_matrix::Color> scratch_;
public:
    std::string get_name() const override { return "shockwave"; }
    void apply(FrameCanvas* canvas, const PostProcessEffect& effect) override;
};
