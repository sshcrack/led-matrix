//
// Created by hendrik on 2/5/25.
//

#pragma once

#include "Scene.h"
#include "plugin/main.h"


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
        ~PingPongGameScene() override = default;

        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        void register_properties() override;
        void load_properties(const nlohmann::json &j) override;

        [[nodiscard]] string get_name() const override;

        using Scene::Scene;

        tmillis_t get_default_duration() override {
            return 15000;
        }

        int get_default_weight() override {
            return 1;
        }
    };

    class PingPongGameSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
