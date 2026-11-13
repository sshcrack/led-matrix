#pragma once

#include <shared/matrix/media_artwork_state.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <future>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "matrix/manager/state.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/PropertyMacros.h"
#include "shared/matrix/utils/utils.h"
#include "shared/matrix/wrappers.h"

namespace Scenes {
/// Base of every Spotify artwork scene: follows the playing track, loads and
/// pre-converts its cover on a worker thread (Magick), cross-fades between
/// tracks and tracks the beat (live analysis when available, otherwise a beat
/// simulated from the track BPM). Subclasses only draw.
class SpotifyArtworkScene : public Scene {
public:
    struct Rgb {
        float r = 0, g = 0, b = 0;
    };
    /// Pre-converted artwork for one track.
    struct Artwork {
        std::string track_id;
        int cover_size = 0;
        std::vector<Rgb> cover;  // cover_size², crisp
        int backdrop_size = 0;
        std::vector<Rgb> backdrop;  // backdrop_size², heavily downsampled
        MediaArtworkState::Palette palette{};
        std::array<Rgb, MediaArtworkState::PaletteSize> accent{};  // vivid versions of the palette
        float bpm = 120.0f;
    };
    struct ArtworkOptions {
        int width, height, cover_size, backdrop_size;
    };

    ~SpotifyArtworkScene() override;

    bool render(rgb_matrix::FrameCanvas* canvas) final;
    [[nodiscard]] int get_weight() const override;
    std::string get_category() const override { return "Media"; }
    int get_default_weight() override { return 3; }

    SceneCapabilities get_capabilities() const override
    {
        auto caps = Scene::get_capabilities();
        caps.requires_network = true;
        caps.supports_audio = true;
        return caps;
    }

    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override
    {
        return Previews::SceneSpec::with_inputs({Previews::Inputs::SpotifyPlayback, Previews::Inputs::Audio});
    }

    [[nodiscard]] SceneInputSpec get_runtime_input_spec() const override
    {
        SceneInputSpec spec;
        spec.require(RuntimeInputIds::SpotifyPlayback);
        return spec;
    }

protected:
    PropertyPointer<int> scene_weight_if_new_song = MAKE_PROPERTY("scene_weight_if_new_song", int, 100);

    /// Sizes the worker prepares the artwork in, read when a new track starts.
    [[nodiscard]] virtual ArtworkOptions artwork_options() const = 0;
    /// Draws one frame. current() is set; beat state is already advanced.
    virtual void draw(rgb_matrix::FrameCanvas* canvas, float dt) = 0;
    /// Called when a different track's artwork becomes current.
    virtual void on_track_changed() {}

    [[nodiscard]] const Artwork& current() const { return *current_; }
    [[nodiscard]] const Artwork* previous() const { return previous_ ? &*previous_ : nullptr; }
    /// 0 = new track just arrived, 1 = settled (previous() is gone at 1).
    [[nodiscard]] float transition() const { return transition_; }
    [[nodiscard]] float progress();
    /// Duration of the cross-fade between two tracks in seconds.
    virtual float transition_seconds() const { return 1.1f; }

    // Music state, updated before each draw().
    float time_ = 0.0f;
    float pulse_ = 0.0f;           // beat envelope 0..1
    float level_ = 0.0f;           // smoothed loudness 0..1
    bool beat_ = false;            // a beat landed this frame
    bool live_ = false;            // real audio analysis is available
    std::vector<float> spectrum_;  // live spectrum (empty without analysis)

    std::vector<Rgb> frame_;
    float random01();
    void present(rgb_matrix::FrameCanvas* canvas);

private:
    static std::expected<Artwork, std::string> refresh_info(SpotifyTrack track, ArtworkOptions options, std::stop_token stop,
                                                            std::promise<Artwork> cover_ready);
    void adopt(Artwork artwork);
    void update_music(float dt);

    // Worker plumbing. Only the render thread mutates scene state; the worker
    // publishes complete results through the futures.
    std::optional<SpotifyState> curr_state;
    std::future<std::expected<Artwork, std::string>> refresh_future;
    std::future<Artwork> cover_future;
    std::stop_source artwork_stop;
    std::optional<std::string> requested_track_id;

    std::optional<Artwork> current_;
    std::optional<Artwork> previous_;
    float transition_ = 1.0f;
    float beat_phase_ = 0.0f;
    uint64_t seen_beat_ = 0;
    bool beat_primed_ = false;
    uint32_t rng_ = 0x5EED1234u;
    std::vector<uint8_t> rgb_;  // reused RGB888 storage for the final bulk write
};

namespace ArtworkPaint {
using Rgb = SpotifyArtworkScene::Rgb;
constexpr float Pi = 3.14159265358979323846f;

inline Rgb operator+(Rgb a, Rgb b)
{
    return {a.r + b.r, a.g + b.g, a.b + b.b};
}
inline Rgb operator*(Rgb a, float s)
{
    return {a.r * s, a.g * s, a.b * s};
}
inline Rgb mix(Rgb a, Rgb b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
inline float luma(Rgb c)
{
    return (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) / 255.0f;
}

inline float smooth01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// Ease-out with a small overshoot: a new cover "lands".
inline float ease_out_back(float t)
{
    t = std::clamp(t, 0.0f, 1.0f) - 1.0f;
    constexpr float k = 1.6f;
    return 1.0f + t * t * ((k + 1.0f) * t + k);
}

/// A colour on the artwork's accent gradient (pos wraps at 1).
inline Rgb sample_accent(const SpotifyArtworkScene::Artwork& art, float pos)
{
    pos = pos - std::floor(pos);
    const float f = pos * static_cast<float>(art.accent.size());
    const auto i0 = static_cast<size_t>(f) % art.accent.size();
    const auto i1 = (i0 + 1) % art.accent.size();
    return mix(art.accent[i0], art.accent[i1], smooth01(f - std::floor(f)));
}

inline Rgb sample_bilinear(const std::vector<Rgb>& img, int size, float u, float v)
{
    const float x = std::clamp(u * size - 0.5f, 0.0f, size - 1.0f);
    const float y = std::clamp(v * size - 0.5f, 0.0f, size - 1.0f);
    const int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, size - 1), y1 = std::min(y0 + 1, size - 1);
    const float fx = x - x0, fy = y - y0;
    const auto at = [&](int xx, int yy) { return img[static_cast<size_t>(yy * size + xx)]; };
    return mix(mix(at(x0, y0), at(x1, y0), fx), mix(at(x0, y1), at(x1, y1), fx), fy);
}

inline Rgb sample_nearest(const std::vector<Rgb>& img, int size, float u, float v)
{
    const int x = std::clamp(static_cast<int>(u * size), 0, size - 1);
    const int y = std::clamp(static_cast<int>(v * size), 0, size - 1);
    return img[static_cast<size_t>(y * size + x)];
}
}  // namespace ArtworkPaint
}  // namespace Scenes
