//
// Created by hendrik on 2/5/25.
//

#pragma once

#include "Scene.h"
#include "plugin.h"


namespace Scenes {
    class PingPongGameScene : public Scene {
    private:
        int ball_size;
        int paddle_width;
        int paddle_height;
        float ball_speed;
        float paddle_speed;
        float speed_multiplier = 1.0f;
        float max_speed_multiplier = 3.0f;
        
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
        explicit PingPongGameScene(const nlohmann::json &config);

        bool render(rgb_matrix::RGBMatrix *matrix) override;
        void initialize(rgb_matrix::RGBMatrix *matrix) override;

        [[nodiscard]] string get_name() const override;

        using Scene::Scene;
    };

    class PingPongGameSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;

        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}
