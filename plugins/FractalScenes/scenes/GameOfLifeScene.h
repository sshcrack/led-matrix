#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <chrono>
#include <vector>
#include <random>

namespace Scenes {
    class GameOfLifeSceneWrapper final : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scene> create() override;
    };

    class GameOfLifeScene final : public Scene {
    public:
        GameOfLifeScene();

        ~GameOfLifeScene() override = default;

        void initialize(int width, int height) override;

        bool render(rgb_matrix::FrameCanvas *canvas) override;

        string get_name() const override;
        std::string get_category() const override { return "Fractals"; }

        tmillis_t get_default_duration() override {
            return 30000;
        }

        int get_default_weight() override {
            return 1;
        }
    protected:
        void register_properties() override;

        void load_properties(const json &j) override;

    private:
        std::chrono::steady_clock::time_point last_update;
        std::vector<bool> current_grid;
        std::vector<bool> next_grid;
        std::vector<int> cell_ages;
        std::vector<float> afterglow;
        float update_interval = 0.2f; // seconds between updates
        float accumulated_time = 0.0f;
        int width = 0;
        int height = 0;
        int steps_since_reset = 0;
        int steps_since_change = 0;
        bool has_changed = false;
        int previous_population = 0;
        std::mt19937 rng{std::random_device{}()};

        // Game of Life rules
        int count_neighbors(int x, int y);

        void update_simulation();

        void reset_simulation(bool randomize = true);

        bool is_stable();

        // Display parameters
        PropertyPointer<int> update_rate = MAKE_PROPERTY_MINMAX("update_rate", int, 5, 1, 20);
        PropertyPointer<float> random_fill = MAKE_PROPERTY_MINMAX("random_fill", float, 0.25f, 0.05f, 0.5f);
        PropertyPointer<int> auto_reset = MAKE_PROPERTY_MINMAX("auto_reset", int, 200, 0, 1000);
        PropertyPointer<bool> age_coloring = MAKE_PROPERTY("age_coloring", bool, true);
        PropertyPointer<bool> afterglow_enabled = MAKE_PROPERTY("afterglow", bool, true);
        PropertyPointer<float> afterglow_decay = MAKE_PROPERTY_MINMAX("afterglow_decay", float, 0.82f, 0.4f, 0.98f);
        PropertyPointer<bool> inject_patterns = MAKE_PROPERTY("inject_patterns", bool, true);
        PropertyPointer<bool> seeded_resets = MAKE_PROPERTY("seeded_resets", bool, true);
        PropertyPointer<int> pattern_injection_rate = MAKE_PROPERTY_MINMAX("pattern_injection_rate", int, 73, 20, 300);

        // Color functions
        void get_cell_color(int age, uint8_t &r, uint8_t &g, uint8_t &b) const;
        void inject_glider(int x, int y, int rotation);
        void inject_r_pentomino(int x, int y);
        void inject_acorn(int x, int y);
    };
}
