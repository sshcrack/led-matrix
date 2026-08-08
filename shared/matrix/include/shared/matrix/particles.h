#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace Scenes::Particles {

/// Common kinematic state for lightweight CPU particle scenes. The struct is
/// deliberately plain-data so individual scenes can derive from it and add
/// colour/phase metadata without paying for virtual dispatch per particle.
struct KinematicParticle {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float life = 0.0f;
    float maxLife = 1.0f;
    float size = 1.0f;
    float rotation = 0.0f;
};

inline void integrate(KinematicParticle &particle, float dt, float ax = 0.0f, float ay = 0.0f)
{
    particle.vx += ax * dt;
    particle.vy += ay * dt;
    particle.x += particle.vx * dt;
    particle.y += particle.vy * dt;
    particle.life -= dt;
}

inline float life_ratio(const KinematicParticle &particle)
{
    if (particle.maxLife <= 0.0f) return 0.0f;
    return std::clamp(particle.life / particle.maxLife, 0.0f, 1.0f);
}

/// Reusable, allocation-stable particle storage. set_limit() reserves once and
/// trims only when adaptive quality lowers a scene's budget. try_push() makes
/// capacity enforcement uniform across particle-based plugins.
template <typename Particle>
class ParticlePool {
public:
    explicit ParticlePool(std::size_t limit = 0) { set_limit(limit); }

    void set_limit(std::size_t limit)
    {
        limit_ = limit;
        if (particles_.size() > limit_) particles_.resize(limit_);
        if (particles_.capacity() < limit_) particles_.reserve(limit_);
    }

    [[nodiscard]] std::size_t limit() const { return limit_; }
    [[nodiscard]] std::size_t size() const { return particles_.size(); }
    [[nodiscard]] bool empty() const { return particles_.empty(); }

    void clear() { particles_.clear(); }
    void resize(std::size_t count) { particles_.resize(std::min(count, limit_)); }

    bool try_push(const Particle &particle)
    {
        if (particles_.size() >= limit_) return false;
        particles_.push_back(particle);
        return true;
    }

    bool try_push(Particle &&particle)
    {
        if (particles_.size() >= limit_) return false;
        particles_.push_back(std::move(particle));
        return true;
    }

    template <typename... Args>
    Particle *try_emplace(Args &&...args)
    {
        if (particles_.size() >= limit_) return nullptr;
        return &particles_.emplace_back(std::forward<Args>(args)...);
    }

    template <typename Predicate>
    void erase_if(Predicate &&predicate)
    {
        std::erase_if(particles_, std::forward<Predicate>(predicate));
    }

    auto begin() { return particles_.begin(); }
    auto end() { return particles_.end(); }
    auto begin() const { return particles_.begin(); }
    auto end() const { return particles_.end(); }

private:
    std::vector<Particle> particles_;
    std::size_t limit_ = 0;
};

} // namespace Scenes::Particles
