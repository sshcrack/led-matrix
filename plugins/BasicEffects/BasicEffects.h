#pragma once

#include "shared/matrix/plugin/main.h"
#include "shared/matrix/post_processing_effect.h"

namespace Plugins {
    class BasicEffects : public BasicPlugin {
    public:
        BasicEffects();
        ~BasicEffects() override = default;

        vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>> create_image_providers() override;
        vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> create_scenes() override;
        
        std::optional<string> before_server_init() override;
        std::string get_plugin_name() const override;
    };
}

// Flash effect implementation
class FlashEffect : public PostProcessingEffect {
public:
    std::string get_name() const override { return "flash"; }
    void apply(FrameCanvas* canvas, const PostProcessEffect& effect) override;
};

// Rotate effect implementation  
class RotateEffect : public PostProcessingEffect {
public:
    std::string get_name() const override { return "rotate"; }
    void apply(FrameCanvas* canvas, const PostProcessEffect& effect) override;
};