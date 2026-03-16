#pragma once

#include "shared/matrix/plugin/main.h"
#include "shared/matrix/transition_effect.h"

namespace Plugins
{
    class Transitions : public BasicPlugin
    {
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
class BlendTransition : public TransitionEffect
{
public:
    std::string get_name() const override { return "blend"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};

// ─── Swipe ────────────────────────────────────────────────────────────────────
/// Horizontal wipe: the incoming scene slides in from the right.
class SwipeTransition : public TransitionEffect
{
public:
    std::string get_name() const override { return "swipe"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};

// ─── Morph ────────────────────────────────────────────────────────────────────
/// Luminance-weighted reveal: bright pixels of 'to' appear before dark ones.
class MorphTransition : public TransitionEffect
{
public:
    std::string get_name() const override { return "morph"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};

// ─── Radial Reveal ───────────────────────────────────────────────────────────
/// Circular reveal from center with a soft edge.
class RadialRevealTransition : public TransitionEffect
{
public:
    std::string get_name() const override { return "radial_reveal"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};

// ─── Checker Reveal ──────────────────────────────────────────────────────────
/// Checkerboard-style staged reveal using 8x8 tiles.
class CheckerRevealTransition : public TransitionEffect
{
public:
    std::string get_name() const override { return "checker_reveal"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};

// ─── Ordered Dissolve ────────────────────────────────────────────────────────
/// Deterministic dissolve using an 8x8 Bayer threshold pattern.
class OrderedDissolveTransition : public TransitionEffect
{
public:
    std::string get_name() const override { return "ordered_dissolve"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};

// ─── Random Dissolve ─────────────────────────────────────────────────────────
/// Hash-based pseudo-random dissolve with deterministic per-pixel ordering.
class RandomDissolveTransition : public TransitionEffect
{
public:
    std::string get_name() const override { return "random_dissolve"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};

// ─── Zoom Blend ──────────────────────────────────────────────────────────────
/// Incoming scene zooms into place while blending from the current scene.
class ZoomBlendTransition : public TransitionEffect
{
public:
    std::string get_name() const override { return "zoom_blend"; }
    void apply(FrameCanvas *dst, FrameCanvas *from, FrameCanvas *to,
               float alpha, int width, int height) override;
};
