#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <future>
#include <optional>
#include <stop_token>
#include <vector>

#include <shared/matrix/media_artwork_state.h>

#include "shared/matrix/Scene.h"
#include "shared/matrix/utils/utils.h"
#include "shared/matrix/wrappers.h"
#include "nlohmann/json.hpp"

#include "matrix/manager/state.h"
#include "shared/matrix/plugin/PropertyMacros.h"

namespace Scenes {
    /// "spotify": the current album cover, staged in a living backdrop that is
    /// derived from the artwork itself. The backdrop is a slowly flowing, blurred
    /// copy of the cover; a spectrum aura, beat sparkles and the progress bar all
    /// take their colours from the cover palette. With live music analysis the
    /// cover reacts to real beats, otherwise to a beat simulated from the BPM.
    ///
    /// Artwork decoding/scaling happens on a worker thread (Magick); the render
    /// thread only works on small pre-converted RGB buffers.
    class CoverOnlyScene final : public Scene {
    public:
        struct Rgb {
            float r = 0, g = 0, b = 0;
        };
        /// Pre-converted artwork for one track.
        struct Artwork {
            std::string track_id;
            int cover_size = 0;
            std::vector<Rgb> cover;       // cover_size², crisp
            int backdrop_size = 0;
            std::vector<Rgb> backdrop;    // backdrop_size², heavily downsampled
            MediaArtworkState::Palette palette{};
            std::array<Rgb, MediaArtworkState::PaletteSize> accent{};  // vivid versions of the palette
            float bpm = 120.0f;
        };

    private:
        PropertyPointer<float> cover_border_glow_intensity =
                MAKE_PROPERTY_MINMAX("cover_border_glow_intensity", float, 0.35f, 0.0f, 1.0f);
        PropertyPointer<int> cover_size_percent = MAKE_PROPERTY_MINMAX("cover_size_percent", int, 66, 45, 94);
        PropertyPointer<int> background_blur = MAKE_PROPERTY_MINMAX("background_blur", int, 8, 0, 20);
        PropertyPointer<int> background_brightness = MAKE_PROPERTY_MINMAX("background_brightness", int, 42, 10, 85);
        PropertyPointer<float> background_motion = MAKE_PROPERTY_MINMAX("background_motion", float, 1.0f, 0.0f, 2.5f);
        PropertyPointer<int> progress_bar_height = MAKE_PROPERTY_MINMAX("progress_bar_height", int, 2, 1, 8);
        PropertyPointer<float> beat_pulse_strength = MAKE_PROPERTY_MINMAX("beat_pulse_strength", float, 0.25f, 0.0f, 0.6f);
        PropertyPointer<bool> show_progress = MAKE_PROPERTY("show_progress", bool, true);
        PropertyPointer<bool> spectrum_aura = MAKE_PROPERTY("spectrum_aura", bool, true);
        PropertyPointer<bool> beat_sparkles = MAKE_PROPERTY("beat_sparkles", bool, true);
        PropertyPointer<int> scene_weight_if_new_song = MAKE_PROPERTY("scene_weight_if_new_song", int, 100);

        struct ArtworkOptions {
            int width, height, cover_size, backdrop_size;
        };
        using ArtworkResult = Artwork;

        static std::expected<ArtworkResult, std::string> refresh_info(
            SpotifyTrack track, ArtworkOptions options, std::stop_token stop,
            std::promise<ArtworkResult> cover_ready);

        // Worker plumbing. Only the render thread mutates scene state; the worker
        // publishes complete results through the futures.
        std::optional<SpotifyState> curr_state;
        std::future<std::expected<ArtworkResult, std::string>> refresh_future;
        std::future<ArtworkResult> cover_future;
        std::stop_source artwork_stop;
        std::optional<std::string> requested_track_id;

        // Presentation state.
        std::optional<Artwork> current_;
        std::optional<Artwork> previous_;     // fading out during a track change
        float transition_ = 1.0f;             // 0 = new track just arrived, 1 = settled
        float time_ = 0.0f;
        float beat_phase_ = 0.0f;             // simulated beat (no live analysis)
        float pulse_ = 0.0f;                  // beat envelope 0..1
        float level_ = 0.0f;                  // smoothed loudness 0..1
        std::vector<float> aura_;             // smoothed per-segment aura length
        std::vector<float> aura_peak_;        // per-segment auto-gain
        uint64_t seen_beat_ = 0;
        bool beat_primed_ = false;
        struct Sparkle {
            float x, y, vx, vy, life, max_life;
            int color;
        };
        std::vector<Sparkle> sparkles_;
        uint32_t rng_ = 0x5EED1234u;
        std::vector<Rgb> frame_;
        std::vector<Rgb> grid_;               // backdrop samples (coarse on the Pi)

        void adopt(Artwork artwork);
        void draw(rgb_matrix::FrameCanvas *canvas, float dt);
        float random01();

    public:
        CoverOnlyScene() = default;
        ~CoverOnlyScene() override;

        bool render(rgb_matrix::FrameCanvas *canvas) override;

        [[nodiscard]] int get_weight() const override;
        [[nodiscard]] string get_name() const override;
        std::string get_category() const override { return "Media"; }
        [[nodiscard]] SceneDescriptor get_descriptor() const override;
        void register_properties() override;

        tmillis_t get_default_duration() override { return 25000; }
        int get_default_weight() override { return 3; }

        SceneCapabilities get_capabilities() const override {
            auto caps = Scene::get_capabilities();
            caps.requires_network = true;
            caps.supports_audio = true;
            // Pi: coarse-grid backdrop; desktop worker: per-pixel backdrop.
            caps.prefers_remote_rendering = true;
            return caps;
        }

        [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
            return Previews::SceneSpec::with_inputs({Previews::Inputs::SpotifyPlayback, Previews::Inputs::Audio});
        }

        [[nodiscard]] SceneInputSpec get_runtime_input_spec() const override {
            SceneInputSpec spec;
            spec.require(RuntimeInputIds::SpotifyPlayback);
            return spec;
        }
    };

    class CoverOnlySceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
