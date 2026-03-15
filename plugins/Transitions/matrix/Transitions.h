#pragma once

#include "shared/matrix/plugin/main.h"
#include "shared/matrix/transition_effect.h"

namespace Plugins {
    class Transitions : public BasicPlugin {
    public:
        Transitions() = default;
        ~Transitions() override = default;

        vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>>
        create_image_providers() override { return {}; }

        vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>>
        create_scenes() override { return {}; }

        std::string get_plugin_name() const override { return "Transitions"; }

        vector<std::unique_ptr<TransitionEffect, void (*)(TransitionEffect *)>>
        create_transitions() override;
    };
}

// ─── Blend ────────────────────────────────────────────────────────────────────
/// Linear pixel crossfade between two canvases.
class BlendTransition : public TransitionEffect {
public:
    std::string get_name() const override { return "blend"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};

// ─── Swipe ────────────────────────────────────────────────────────────────────
/// Horizontal wipe: the incoming scene slides in from the right.
class SwipeTransition : public TransitionEffect {
public:
    std::string get_name() const override { return "swipe"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};

// ─── Morph ────────────────────────────────────────────────────────────────────
/// Luminance-weighted reveal: bright pixels of 'to' appear before dark ones.
class MorphTransition : public TransitionEffect {
public:
    std::string get_name() const override { return "morph"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};
