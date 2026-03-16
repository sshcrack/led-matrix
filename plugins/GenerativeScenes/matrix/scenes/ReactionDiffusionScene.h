#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <array>
#include <vector>
#include <random>

namespace GenerativeScenes {

    // Gray-Scott reaction-diffusion model.
    // Two virtual chemicals U and V interact and diffuse, spontaneously growing
    // Turing patterns: spots, labyrinths, stripes, coral — cycling over time.
    class ReactionDiffusionScene : public Scenes::Scene {
    public:
        ReactionDiffusionScene();
        ~ReactionDiffusionScene() override = default;

        void initialize(int width, int height) override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void register_properties() override;

        std::string get_name() const override { return "reaction_diffusion"; }
        tmillis_t get_default_duration() override { return 60000; }
        int get_default_weight() override { return 3; }

    private:
        // Simulation grid (double-buffered)
        static constexpr int MAX_W = 128;
        static constexpr int MAX_H = 128;

        std::array<float, MAX_W * MAX_H> u_cur{}, v_cur{};
        std::array<float, MAX_W * MAX_H> u_nxt{}, v_nxt{};

        // Gray-Scott parameter preset
        struct Preset {
            float F, k;
            const char *name;
        };

        static constexpr Preset PRESETS[] = {
            { 0.035f, 0.065f, "spots"      },
            { 0.025f, 0.060f, "mitosis"    },
            { 0.029f, 0.057f, "maze"       },
            { 0.039f, 0.058f, "bubbles"    },
            { 0.060f, 0.062f, "stripes"    },
        };
        static constexpr int NUM_PRESETS = 5;

        int current_preset = 0;
        int step_count = 0;
        static constexpr int STEPS_PER_PRESET = 4000; // steps before cycling

        // Gray-Scott constants
        static constexpr float DU = 0.16f;
        static constexpr float DV = 0.08f;
        static constexpr float DT = 1.0f;
        static constexpr int   SIM_STEPS_PER_FRAME = 8;

        // Visual
        float global_hue = 0.0f;

        std::mt19937 rng;

        void seed_random_patch();
        void simulation_step(float F, float k);
        static std::tuple<uint8_t, uint8_t, uint8_t> palette(float v, float hue_shift);
    };

    class ReactionDiffusionSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override {
            return {new ReactionDiffusionScene(), [](Scenes::Scene *s) {
                delete (ReactionDiffusionScene *) s;
            }};
        }
    };

}
