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
