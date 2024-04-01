#pragma once

#include "shared/utils/utils.h"
#include <nlohmann/json.hpp>
#include "led-matrix.h"
#include "content-streamer.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;

namespace Scenes {
    class Scene {
    private:
        bool initialized = false;

    protected:
        int weight = 0;
        tmillis_t duration = 0;
        FrameCanvas *offscreen_canvas = nullptr;

    public:
        static nlohmann::json get_config(int weight, tmillis_t duration) {
            return {
                    {"weight",   weight},
                    {"duration", duration}
            };
        }

        explicit Scene(const nlohmann::json &json) {
            weight = json["weight"];
            duration = json["duration"];
        }

        [[nodiscard]] int get_weight() const {
            return weight;

        };

        [[nodiscard]] tmillis_t get_duration() const {
            return duration;
        };

        [[nodiscard]] nlohmann::json to_json() const {
            return {
                    {"weight",   get_weight()},
                    {"duration", get_duration()}
            };
        }

        virtual void initialize(RGBMatrix *matrix) {
            if (initialized)
                return;

            offscreen_canvas = matrix->CreateFrameCanvas();
            initialized = true;
        }

        bool is_initialized() { return initialized; }

        // Returns true if the scene is done and should be removed
        virtual bool tick(RGBMatrix *matrix) = 0;
    };


}