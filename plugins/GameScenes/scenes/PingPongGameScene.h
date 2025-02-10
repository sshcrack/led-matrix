//
// Created by hendrik on 2/5/25.
//

#pragma once

#include "Scene.h"
#include "plugin/main.h"


namespace Scenes {
    class PingPongGameScene : public Scene {
    private:
        Property<int> ball_size = Property("ball_size", 2);
        Property<int> paddle_width = Property("paddle_width", 2);
        Property<int> paddle_height = Property("paddle_height", 8);
        Property<float> ball_speed = Property("ball_speed", 0.3f);
        Property<float> paddle_speed = Property("paddle_speed", 0.15f);
        Property<float> target_fps = Property("target_fps", 60.0f);
        Property<float> speed_multiplier = Property("speed_multiplier", 1.0f);
        Property<float> max_speed_multiplier = Property("max_speed_multiplier", 4.0f);

        float curr_speed_multiplier = 1.0f;

        float ball_x;
        float ball_y;
        float ball_dx;
        float ball_dy;
        
        float left_paddle_y;
        float right_paddle_y;
        
        // Previous positions for clearing
        float prev_ball_x;
        float prev_ball_y;
        float prev_left_paddle_y;
        float prev_right_paddle_y;

        int frame_counter = 0;
        int update_frequency = 3;  // Update every N frames

        // Timing control
        std::chrono::steady_clock::time_point last_update;
        float target_frame_time = 1.0f/60.0f;  // 60 FPS
        float accumulated_time = 0.0f;

    public:
        explicit PingPongGameScene();

        bool render(rgb_matrix::RGBMatrix *matrix) override;
        void initialize(rgb_matrix::RGBMatrix *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        void register_properties() override;
        void load_properties(const nlohmann::json &j) override;

        [[nodiscard]] string get_name() const override;

        using Scene::Scene;
    };

    class PingPongGameSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
