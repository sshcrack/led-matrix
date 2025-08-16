#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "shared/matrix/plugin/property.h"

#include <vector>
#include <random>
#include <chrono>
// rgb matrix types
#include "graphics.h"

namespace Scenes
{
    class CountdownScene : public Scene
    {
    private:
        FrameTimer frameTimer;

        PropertyPointer<int> target_year = MAKE_PROPERTY("target_year", int, 2026);
        PropertyPointer<bool> fireworks = MAKE_PROPERTY("fireworks", bool, true);
        PropertyPointer<bool> confetti = MAKE_PROPERTY("confetti", bool, true);
        PropertyPointer<bool> big_digits = MAKE_PROPERTY("big_digits", bool, true);
        PropertyPointer<float> pulse_speed = MAKE_PROPERTY("pulse_speed", float, 1.0f);
        PropertyPointer<rgb_matrix::Color> digit_color = MAKE_PROPERTY("digit_color", rgb_matrix::Color, rgb_matrix::Color(255, 180, 60));
        PropertyPointer<int> show_seconds = MAKE_PROPERTY("show_seconds", int, 1);
#ifdef ENABLE_EMULATOR
        // Debug property to enable fast-preview/testing mode
        PropertyPointer<bool> debug = MAKE_PROPERTY("debug", bool, false);
#endif

        // Particle structs
        struct Particle
        {
            float x, y;
            float vx, vy;
            rgb_matrix::Color color;
            float life;
        };

        std::vector<Particle> confetti_particles;
        std::vector<Particle> fireworks_particles;

        int width = 0, height = 0;
        std::mt19937 rng;
        // Spawn throttle helpers
        float last_confetti_spawn = -1.0f;
        float last_firework_spawn = -1.0f;

        void spawn_confetti(int count);
        void spawn_firework(float x, float y);
        void update_particles(float dt);

    public:
        CountdownScene();
        ~CountdownScene() override = default;
        bool render(RGBMatrixBase *matrix) override;
        string get_name() const override;
        void register_properties() override;

        tmillis_t get_default_duration() override;
        int get_default_weight() override;
        int get_weight() const override;
    };

    class CountdownSceneWrapper : public Plugins::SceneWrapper
    {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
