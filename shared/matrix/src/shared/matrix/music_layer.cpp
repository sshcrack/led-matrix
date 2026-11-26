#include <shared/matrix/audio_state.h>
#include <shared/matrix/input_ids.h>
#include <shared/matrix/music_layer.h>

#include <algorithm>
#include <cmath>

namespace MusicLayer {

bool globally_enabled(const RuntimeInputs::Snapshot& snapshot)
{
    return snapshot.boolean(RuntimeInputIds::MusicLayer, "enabled").value_or(true);
}

void publish_global(bool enabled)
{
    RuntimeInputs::set_available(RuntimeInputIds::MusicLayer, true, {{"enabled", enabled}});
}

bool music_playing(const RuntimeInputs::Snapshot& snapshot)
{
    if (snapshot.available(RuntimeInputIds::SpotifyPlayback) &&
        snapshot.boolean(RuntimeInputIds::SpotifyPlayback, "playing").value_or(true))
        return true;
    return snapshot.available(RuntimeInputIds::Audio) && !snapshot.boolean(RuntimeInputIds::Audio, "silence").value_or(false);
}

bool live(const RuntimeInputs::Snapshot& snapshot)
{
    return globally_enabled(snapshot) && music_playing(snapshot);
}

Layer::Layer() : choice_(MAKE_ENUM_PROPERTY("music_layer", Choice, Choice::AUTO))
{
    choice_->label("Music bars")
        .description("Spectrum bars behind the scene while music plays. Auto follows the global switch.")
        .group("Music");
}

void Layer::describe(Scenes::SceneDescriptor& descriptor) const
{
    if (!switched_off())
        descriptor.tags.emplace_back(Tag);
}

float Layer::update(float dt)
{
    dt = std::clamp(dt, 0.0f, 0.25f);
    clock_ += dt;
    poll_ -= dt;
    if (!snapped_ || poll_ <= 0.0f) {
        wanted_ = !switched_off() && live(RuntimeInputs::snapshot());
        poll_ = 0.25f;
    }

    const auto audio = AudioState::snapshot();
    const bool fresh = audio.fresh();
    if (fresh || fade_ > 0.0f || wanted_)
        spectrum_.update(audio, dt);
    if (!snapped_) {
        fade_ = wanted_ ? 1.0f : 0.0f;
        audio_mix_ = fresh ? 1.0f : 0.0f;
        snapped_ = true;
    }
    // ~1.5 s in when the music starts, ~3 s out after it stops.
    fade_ = std::clamp(fade_ + (wanted_ ? dt / 1.5f : -dt / 3.0f), 0.0f, 1.0f);
    audio_mix_ = std::clamp(audio_mix_ + (fresh ? dt / 0.8f : -dt / 2.0f), 0.0f, 1.0f);

    // Spotify without audio capture: a slow, low swell instead of flat bars,
    // heavier in the low bands like a real spectrum.
    const int n = static_cast<int>(spectrum_.value.size());
    idle_.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(std::max(1, n - 1));
        const float swell = 0.5f + 0.5f * std::sin(clock_ * 0.9f + u * 7.0f) * std::sin(clock_ * 0.37f - u * 3.1f);
        idle_[static_cast<std::size_t>(i)] = (0.16f + 0.22f * swell) * (1.0f - 0.45f * u);
    }
    return fade_;
}

void Layer::paint(const Environment::Surface& s, const std::vector<std::uint8_t>* mask, const Placement& p) const
{
    const float strength = fade_ * std::clamp(p.brightness, 0.0f, 1.0f);
    if (strength <= 0.003f || s.px == nullptr)
        return;
    const int pitch = std::max(1, p.bar + p.gap);
    const int count = (s.w + p.gap) / pitch;
    const int bins = static_cast<int>(spectrum_.value.size());
    if (count <= 0 || bins <= 0)
        return;
    const int baseRow = std::clamp(static_cast<int>(std::lround(p.base)), 0, s.h);
    const bool masked = mask != nullptr && mask->size() >= static_cast<std::size_t>(s.w) * static_cast<std::size_t>(s.h);

    const auto level = [&](int i, bool peak) {
        const int b = std::clamp(i * bins / count, 0, bins - 1);
        const auto idx = static_cast<std::size_t>(b);
        const float live = peak ? spectrum_.peak[idx] : spectrum_.value[idx];
        return idle_[idx] + (live - idle_[idx]) * audio_mix_;
    };
    const auto column = [&](int x, float v, float peakV, Environment::Rgb tint, float gain) {
        if (x < 0 || x >= s.w)
            return;
        const float height = std::clamp(v, 0.0f, 1.0f) * p.max_height;
        const int top = std::max(0, static_cast<int>(std::lround(p.base - height)));
        const float invH = 1.0f / std::max(1.0f, height);
        for (int y = top; y < baseRow; ++y) {
            if (masked && !(*mask)[static_cast<std::size_t>(y * s.w + x)])
                continue;
            // Brighter towards the tip, like light rising off the horizon.
            const float depth = (p.base - (static_cast<float>(y) + 0.5f)) * invH;
            s.add(x, y, tint, gain * (0.4f + 0.6f * std::clamp(depth, 0.0f, 1.0f)));
        }
        if (peakV > 0.02f) {
            const int dot = static_cast<int>(std::lround(p.base - 1.0f - std::clamp(peakV, 0.0f, 1.0f) * p.max_height));
            if (dot >= 0 && dot < baseRow && (!masked || (*mask)[static_cast<std::size_t>(dot * s.w + x)]))
                s.add(x, dot, tint, gain * 1.15f);
        }
    };

    for (int i = 0; i < count; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(std::max(1, count - 1));
        const Environment::Rgb tint = Environment::mix(p.low, p.high, u);
        const float v = level(i, false), peakV = level(i, true);
        for (int dx = 0; dx < p.bar; ++dx) {
            const int x = i * pitch + dx;
            // The dim mirrored layer first, the main bars on top.
            column(s.w - 1 - x, v, peakV, p.mirror, strength * 0.42f);
            column(x, v, peakV, tint, strength);
        }
    }
}

}  // namespace MusicLayer
