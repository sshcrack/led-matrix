#pragma once

#include <algorithm>
#include <array>
#include <cmath>

class SectionTracker {
public:
    bool update(const std::array<float, 7>& bands, float dt, bool active, float beat_period)
    {
        if (!active) {
            *this = SectionTracker{};
            return false;
        }
        if (!std::isfinite(dt) || dt <= 0.0f)
            return false;
        for (float value : bands)
            if (!std::isfinite(value)) return false;
        dt = std::min(dt, 0.1f);
        if (!primed_) {
            recent_ = reference_ = bands;
            primed_ = true;
        }
        age_ += dt;
        cooldown_ = std::max(0.0f, cooldown_ - dt);
        float distance = 0.0f;
        const float recent_alpha = 1.0f - std::exp(-dt / 0.35f);
        for (std::size_t i = 0; i < bands.size(); ++i) {
            recent_[i] += (std::clamp(bands[i], 0.0f, 1.0f) - recent_[i]) * recent_alpha;
            const float delta = recent_[i] - reference_[i];
            distance += delta * delta;
        }
        distance = std::sqrt(distance / static_cast<float>(bands.size()));
        const bool novel = distance > 0.20f && age_ >= 5.0f && cooldown_ <= 0.0f;
        evidence_ = novel ? evidence_ + dt : std::max(0.0f, evidence_ - dt * 2.0f);
        const float confirmation = std::isfinite(beat_period) ? std::clamp(beat_period * 1.5f, 0.65f, 1.2f) : 0.8f;
        if (evidence_ >= confirmation) {
            reference_ = recent_;
            evidence_ = 0.0f;
            cooldown_ = 6.0f;
            return true;
        }
        const float reference_alpha = 1.0f - std::exp(-dt / (age_ < 5.0f ? 0.8f : 8.0f));
        if (!novel)
            for (std::size_t i = 0; i < bands.size(); ++i)
                reference_[i] += (recent_[i] - reference_[i]) * reference_alpha;
        return false;
    }

private:
    std::array<float, 7> recent_{};
    std::array<float, 7> reference_{};
    float age_ = 0.0f;
    float evidence_ = 0.0f;
    float cooldown_ = 0.0f;
    bool primed_ = false;
};
