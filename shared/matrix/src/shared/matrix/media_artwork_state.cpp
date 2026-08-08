#include "shared/matrix/media_artwork_state.h"

#include <algorithm>
#include <cmath>

namespace MediaArtworkState {
namespace {
std::mutex mutex;
Palette current{};
Palette previous{};
std::string current_key;
std::uint64_t generation = 0;
bool valid = false;
std::chrono::steady_clock::time_point changed_at{};
constexpr float TransitionSeconds = 1.8f;

uint8_t blend_channel(uint8_t from, uint8_t to, float amount)
{
    return static_cast<uint8_t>(std::clamp(
        static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * amount,
        0.0f, 255.0f));
}

rgb_matrix::Color blend(const rgb_matrix::Color &from, const rgb_matrix::Color &to, float amount)
{
    return {
        blend_channel(from.r, to.r, amount),
        blend_channel(from.g, to.g, amount),
        blend_channel(from.b, to.b, amount),
    };
}
}

void update(const std::string &media_key, const Palette &colors)
{
    if (media_key.empty()) return;
    std::lock_guard lock(mutex);
    if (valid && current_key == media_key) return;
    previous = valid ? current : colors;
    current = colors;
    current_key = media_key;
    valid = true;
    ++generation;
    changed_at = std::chrono::steady_clock::now();
}

void clear()
{
    std::lock_guard lock(mutex);
    valid = false;
    current_key.clear();
    ++generation;
}

Snapshot snapshot()
{
    std::lock_guard lock(mutex);
    Snapshot result;
    result.valid = valid;
    result.generation = generation;
    if (!valid) return result;

    const float elapsed = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - changed_at).count();
    const float linear = std::clamp(elapsed / TransitionSeconds, 0.0f, 1.0f);
    // Smoothstep avoids a visible velocity discontinuity at either end.
    const float amount = linear * linear * (3.0f - 2.0f * linear);
    result.transition = amount;
    for (std::size_t i = 0; i < PaletteSize; ++i)
        result.colors[i] = blend(previous[i], current[i], amount);
    return result;
}

rgb_matrix::Color sample(const Snapshot &artwork, float position)
{
    if (!artwork.valid) return {};
    const float wrapped = position - std::floor(position);
    const float scaled = wrapped * static_cast<float>(PaletteSize);
    const std::size_t first = static_cast<std::size_t>(scaled) % PaletteSize;
    const std::size_t second = (first + 1) % PaletteSize;
    const float amount = scaled - std::floor(scaled);
    return blend(artwork.colors[first], artwork.colors[second], amount);
}

} // namespace MediaArtworkState
