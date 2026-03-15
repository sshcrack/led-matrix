#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {
    class BoidsScene : public Scenes::Scene {
    private:
        struct Vector2 {
            float x, y;

            Vector2() : x(0), y(0) {}
            Vector2(float _x, float _y) : x(_x), y(_y) {}

            Vector2 operator+(const Vector2& v) const { return {x + v.x, y + v.y}; }
            Vector2 operator-(const Vector2& v) const { return {x - v.x, y - v.y}; }
            Vector2 operator*(float s) const { return {x * s, y * s}; }
            Vector2 operator/(float s) const { return {x / s, y / s}; }

            Vector2& operator+=(const Vector2& v) { x += v.x; y += v.y; return *this; }
            Vector2& operator-=(const Vector2& v) { x -= v.x; y -= v.y; return *this; }
            Vector2& operator*=(float s) { x *= s; y *= s; return *this; }
            Vector2& operator/=(float s) { x /= s; y /= s; return *this; }

            float magSq() const { return x * x + y * y; }
            float mag() const { return std::sqrt(magSq()); }

            void normalize() {
                float m = mag();
                if (m > 0.0001f) {
                    x /= m;
                    y /= m;
                }
            }

            void limit(float max) {
                if (magSq() > max * max) {
                    normalize();
                    x *= max;
                    y *= max;
                }
            }
        };

        struct Boid {
            Vector2 position;
            Vector2 velocity;
            Vector2 acceleration;
            uint8_t r, g, b;

            Boid(float x, float y) : position(x, y) {
                acceleration = Vector2(0, 0);
                velocity = Vector2(2.0f * ((float)rand() / RAND_MAX - 0.5f), 
                                   2.0f * ((float)rand() / RAND_MAX - 0.5f));
            }
        };

        std::vector<Boid> flock;

        PropertyPointer<int> num_boids = MAKE_PROPERTY("num_boids", int, 100);
        PropertyPointer<float> max_speed = MAKE_PROPERTY("max_speed", float, 1.0f);
        PropertyPointer<float> max_force = MAKE_PROPERTY("max_force", float, 0.05f);
        PropertyPointer<rgb_matrix::Color> boid_color = MAKE_PROPERTY("boid_color", rgb_matrix::Color, rgb_matrix::Color(255, 255, 255));
        PropertyPointer<bool> use_random_colors = MAKE_PROPERTY("use_random_colors", bool, true);
        PropertyPointer<float> sep_dist = MAKE_PROPERTY("sep_dist", float, 10.0f);
        PropertyPointer<float> ali_dist = MAKE_PROPERTY("ali_dist", float, 25.0f);
        PropertyPointer<float> coh_dist = MAKE_PROPERTY("coh_dist", float, 25.0f);
        PropertyPointer<float> sep_weight = MAKE_PROPERTY("sep_weight", float, 1.5f);
        PropertyPointer<float> ali_weight = MAKE_PROPERTY("ali_weight", float, 1.0f);
        PropertyPointer<float> coh_weight = MAKE_PROPERTY("coh_weight", float, 1.0f);
        PropertyPointer<bool> wraparound = MAKE_PROPERTY("wraparound", bool, true);

        void run_boids();
        void edges(Boid& b);
        void flock_boid(Boid& boid);

        Vector2 separate(Boid& boid);
        Vector2 align(Boid& boid);
        Vector2 cohesion(Boid& boid);

        void hsl_to_rgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b);

    public:
        explicit BoidsScene();
        ~BoidsScene() override = default;

        void register_properties() override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void initialize(int width, int height) override;

        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class BoidsSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}
