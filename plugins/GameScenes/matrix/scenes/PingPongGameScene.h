#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

#include <chrono>
#include <random>

namespace Scenes {
    class PingPongGameScene : public Scene {
    private:
        PropertyPointer<int> ball_size = MAKE_PROPERTY("ball_size", int, 2);
        PropertyPointer<int> paddle_width = MAKE_PROPERTY("paddle_width", int, 2);
        PropertyPointer<int> paddle_height = MAKE_PROPERTY("paddle_height", int, 8);
        PropertyPointer<float> ball_speed = MAKE_PROPERTY("ball_speed", float, 0.3f);
        PropertyPointer<float> paddle_speed = MAKE_PROPERTY("paddle_speed", float, 0.15f);
        PropertyPointer<float> target_fps = MAKE_PROPERTY("target_fps", float, 60.0f);
        PropertyPointer<float> speed_multiplier = MAKE_PROPERTY("speed_multiplier", float, 1.0f);
        PropertyPointer<float> max_speed_multiplier = MAKE_PROPERTY("max_speed_multiplier", float, 4.0f);

        float curr_speed_multiplier = 1.0f;
        float ball_x = 0.0f;
        float ball_y = 0.0f;
        float ball_dx = 0.0f;
        float ball_dy = 0.0f;
        float left_paddle_y = 0.0f;
        float right_paddle_y = 0.0f;
        float left_target_y = 0.0f;
        float right_target_y = 0.0f;
        int left_score = 0;
        int right_score = 0;

        std::chrono::steady_clock::time_point last_update;
        float target_frame_time = 1.0f / 60.0f;
        float accumulated_time = 0.0f;
        std::mt19937 random_engine{std::random_device{}()};

        void reset_ball(int direction);
        void update_game(float dt);
        float predicted_intercept(bool left_side) const;
        void draw_score(rgb_matrix::FrameCanvas *canvas) const;

    public:
        explicit PingPongGameScene();
        ~PingPongGameScene() override = default;

        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void initialize(int width, int height) override;
        void register_properties() override;
        void load_properties(const nlohmann::json &j) override;

        [[nodiscard]] std::string get_name() const override;
        [[nodiscard]] std::string get_category() const override { return "Games"; }

        using Scene::Scene;

        tmillis_t get_default_duration() override { return 15000; }
        int get_default_weight() override { return 1; }
    };

    class PingPongGameSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
