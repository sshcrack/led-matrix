#include "BoidsScene.h"
#include <cmath>

namespace AmbientScenes {
    BoidsScene::BoidsScene() : Scene() {
    }

    void BoidsScene::hsl_to_rgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b) {
        float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = l - c / 2.0f;
        
        float r1 = 0, g1 = 0, b1 = 0;
        if (h >= 0 && h < 60) { r1 = c; g1 = x; b1 = 0; }
        else if (h >= 60 && h < 120) { r1 = x; g1 = c; b1 = 0; }
        else if (h >= 120 && h < 180) { r1 = 0; g1 = c; b1 = x; }
        else if (h >= 180 && h < 240) { r1 = 0; g1 = x; b1 = c; }
        else if (h >= 240 && h < 300) { r1 = x; g1 = 0; b1 = c; }
        else if (h >= 300 && h < 360) { r1 = c; g1 = 0; b1 = x; }
        
        r = static_cast<uint8_t>((r1 + m) * 255.0f);
        g = static_cast<uint8_t>((g1 + m) * 255.0f);
        b = static_cast<uint8_t>((b1 + m) * 255.0f);
    }

    void BoidsScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        matrix_width = matrix_width;
        matrix_height = matrix_height;

        flock.clear();
        for (int i = 0; i < num_boids->get(); i++) {
            Boid b((float)(rand() % matrix_width), (float)(rand() % matrix_height));
            
            // Random color 
            float h = (float)(rand() % 360);
            hsl_to_rgb(h, 1.0f, 0.5f, b.r, b.g, b.b);

            flock.push_back(b);
        }
    }

    bool BoidsScene::render(rgb_matrix::FrameCanvas *canvas) {
        // Fade out slightly to leave trails
        // Boids move fast, let's clear the canvas completely to avoid mess? Or fade.
        // Let's clear for now.
        canvas->Clear();

        run_boids();

        for (auto& b : flock) {
            b.position += b.velocity;
            
            // Draw
            int px = (int)std::round(b.position.x);
            int py = (int)std::round(b.position.y);
            
            if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height) {
                if (use_random_colors->get()) {
                    canvas->SetPixel(px, py, b.r, b.g, b.b);
                } else {
                    auto col = boid_color->get();
                    canvas->SetPixel(px, py, col.r, col.g, col.b);
                }
            }
        }

        wait_until_next_frame();
        return true;
    }

    void BoidsScene::run_boids() {
        // Need to calculate flocking for all first, then apply
        for (auto& b : flock) {
            flock_boid(b);
        }
        for (auto& b : flock) {
            b.velocity += b.acceleration;
            b.velocity.limit(max_speed->get());
            b.acceleration *= 0.0f; // Reset acceleration to 0 each cycle
            edges(b);
        }
    }

    void BoidsScene::flock_boid(Boid& boid) {
        Vector2 sep = separate(boid);   // Separation
        Vector2 ali = align(boid);      // Alignment
        Vector2 coh = cohesion(boid);   // Cohesion

        // Arbitrarily weight these forces
        sep *= sep_weight->get();
        ali *= ali_weight->get();
        coh *= coh_weight->get();

        // Add the force vectors to acceleration
        boid.acceleration += sep;
        boid.acceleration += ali;
        boid.acceleration += coh;
    }

    void BoidsScene::edges(Boid& b) {
        if (wraparound->get()) {
            if (b.position.x > matrix_width) b.position.x = 0;
            else if (b.position.x < 0) b.position.x = (float)matrix_width;

            if (b.position.y > matrix_height) b.position.y = 0;
            else if (b.position.y < 0) b.position.y = (float)matrix_height;
        } else {
            // Bounce
            if (b.position.x >= matrix_width) {
                b.position.x = (float)matrix_width - 1;
                b.velocity.x *= -1;
            } else if (b.position.x < 0) {
                b.position.x = 0;
                b.velocity.x *= -1;
            }

            if (b.position.y >= matrix_height) {
                b.position.y = (float)matrix_height - 1;
                b.velocity.y *= -1;
            } else if (b.position.y < 0) {
                b.position.y = 0;
                b.velocity.y *= -1;
            }
        }
    }

    BoidsScene::Vector2 BoidsScene::separate(Boid& boid) {
        float desiredseparation = sep_dist->get();
        Vector2 steer(0, 0);
        int count = 0;

        for (auto& other : flock) {
            float d = (boid.position - other.position).mag();
            const float epsilon = 0.0001f; // Prevent division by zero
            if ((d > 0) && (d < desiredseparation)) {
                Vector2 diff = boid.position - other.position;
                diff.normalize();
                diff /= (d + epsilon);
                steer += diff;
                count++;
            }
        }
        
        if (count > 0) {
            steer /= (float)count;
            if (steer.magSq() > 0) {
                steer.normalize();
                steer *= max_speed->get();
                steer -= boid.velocity;
                steer.limit(max_force->get());
            }
        }
        return steer;
    }

    BoidsScene::Vector2 BoidsScene::align(Boid& boid) {
        float neighbordist = ali_dist->get();
        Vector2 sum(0, 0);
        int count = 0;

        for (auto& other : flock) {
            float d = (boid.position - other.position).mag();
            if ((d > 0) && (d < neighbordist)) {
                sum += other.velocity;
                count++;
            }
        }
        if (count > 0) {
            sum /= (float)count;
            sum.normalize();
            sum *= max_speed->get();
            Vector2 steer = sum - boid.velocity;
            steer.limit(max_force->get());
            return steer;
        }
        return Vector2(0, 0);
    }

    BoidsScene::Vector2 BoidsScene::cohesion(Boid& boid) {
        float neighbordist = coh_dist->get();
        Vector2 sum(0, 0);
        int count = 0;

        for (auto& other : flock) {
            float d = (boid.position - other.position).mag();
            if ((d > 0) && (d < neighbordist)) {
                sum += other.position;
                count++;
            }
        }
        if (count > 0) {
            sum /= (float)count;
            
            // steer towards the target
            Vector2 desired = sum - boid.position;
            desired.normalize();
            desired *= max_speed->get();
            Vector2 steer = desired - boid.velocity;
            steer.limit(max_force->get());
            return steer;
        }
        return Vector2(0, 0);
    }

    std::string BoidsScene::get_name() const {
        return "boids";
    }

    void BoidsScene::register_properties() {
        add_property(num_boids);
        add_property(boid_color);
        add_property(use_random_colors);
        // We could register others, but these are fine to adjust
        add_property(max_speed);
        add_property(max_force);
        add_property(sep_dist);
        add_property(ali_dist);
        add_property(coh_dist);
        add_property(sep_weight);
        add_property(ali_weight);
        add_property(coh_weight);
        add_property(wraparound); 
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> BoidsSceneWrapper::create() {
        return {new BoidsScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<BoidsScene *>(scene);
        }};
    }
}
