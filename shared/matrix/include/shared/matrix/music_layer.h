#pragma once

// Music layer: spectrum bars that rise behind a scenery scene while music
// plays (the AudioSpectrum look: 2-px bars, a dim mirrored layer, falling peak
// dots), so a calm scene can stay up through a music session without turning
// into a different scene.
//
// A scene owns one Layer, adds property() in register_properties(), calls
// describe() in get_descriptor() and update() + paint() every frame. paint()
// only adds light where the scene's mask allows (open sky), so hills, houses
// and trees stay in front of the bars.
//
// The layer shows while Spotify plays or live audio is non-silent, unless it
// is switched off:
//
//     scene property "music_layer" (off)  >  global switch (config.json → music_layer)
//
// The global switch reaches every process (desktop scene worker included)
// through the "music.layer" Runtime Input. Automatic Mode treats a tagged scene
// whose layer is on as a gentle music look while music plays.

#include <shared/matrix/audio_kit.h>
#include <shared/matrix/environment/paint.h>
#include <shared/matrix/plugin/PropertyMacros.h>
#include <shared/matrix/plugin/property.h>
#include <shared/matrix/runtime_inputs.h>
#include <shared/matrix/scene_descriptor.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace MusicLayer {

/// Descriptor tag of scenes whose layer is not switched off.
inline constexpr std::string_view Tag = "music-layer";

/// Per-scene property value (UPPER_CASE like every other enum property).
enum class Choice { AUTO, OFF };

/// The global switch; true when nothing was published (on by default).
[[nodiscard]] bool globally_enabled(const RuntimeInputs::Snapshot& snapshot);
/// Publishes the global switch (daemon start and POST /music_layer).
void publish_global(bool enabled);
/// Spotify plays, or the live audio analysis is available and not silent.
[[nodiscard]] bool music_playing(const RuntimeInputs::Snapshot& snapshot);
/// globally_enabled() && music_playing(): what Automatic Mode checks for
/// tagged scenes.
[[nodiscard]] bool live(const RuntimeInputs::Snapshot& snapshot);

/// Where and how a scene wants its bars.
struct Placement {
    float base = 64.0f;                            // row (px) the bars rise from, usually the horizon
    float max_height = 50.0f;                      // px a full bar reaches above base
    float brightness = 1.0f;                       // 0..1 overall strength (dim it under a bright day sky)
    int bar = 2, gap = 1;                          // px
    Environment::Rgb low{0.09f, 0.29f, 0.40f};     // main bars, low bands
    Environment::Rgb high{0.18f, 0.22f, 0.47f};    // main bars, high bands
    Environment::Rgb mirror{0.25f, 0.18f, 0.49f};  // the dim mirrored layer
};

class Layer {
public:
    Layer();

    [[nodiscard]] std::shared_ptr<Plugins::PropertyBase> property() const { return choice_; }
    /// Adds Tag unless the scene property is OFF.
    void describe(Scenes::SceneDescriptor& descriptor) const;
    /// The scene property is OFF (false before the property is loaded, e.g. for
    /// a catalogue scan that only reads descriptors).
    [[nodiscard]] bool switched_off() const { return choice_->is_registered() && choice_->get().get() == Choice::OFF; }

    /// Once per frame with the real (unscaled) delta. Fades in when music
    /// starts and out after it stops; returns the visibility 0..1.
    float update(float dt);
    [[nodiscard]] float visibility() const { return fade_; }
    /// Start hidden (or shown if music already plays) instead of fading.
    void snap() { snapped_ = false; }

    /// Adds the bars to `s` above p.base where `mask` (one byte per pixel,
    /// row-major, same size as `s`; null = everywhere) is non-zero. A no-op
    /// while the layer is hidden.
    void paint(const Environment::Surface& s, const std::vector<std::uint8_t>* mask, const Placement& p) const;

private:
    PropertyPointer<Plugins::EnumProperty<Choice>> choice_;
    AudioKit::SpectrumFollower spectrum_{48};
    std::vector<float> idle_;  // calm stand-in motion when Spotify plays without audio capture
    float fade_ = 0.0f;
    float audio_mix_ = 0.0f;  // 0 idle motion .. 1 live spectrum
    float poll_ = 0.0f;
    float clock_ = 0.0f;
    bool wanted_ = false;
    bool snapped_ = false;
};

}  // namespace MusicLayer
