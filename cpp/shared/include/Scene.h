#pragma once

#include "shared/utils/utils.h"
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include "led-matrix.h"
#include "content-streamer.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using std::string;

namespace Scenes {
    class Scene {
    private:
        bool create_offscreen = false;
    protected:
        bool initialized = false;
        /// Only available after initialization
        int matrix_width;
        /// Only available after initialization
        int matrix_height;

        int weight = 0;
        tmillis_t duration = 0;
        rgb_matrix::FrameCanvas *offscreen_canvas = nullptr;

    public:
        static nlohmann::json create_default(int weight, tmillis_t duration);

        explicit Scene(const nlohmann::json &json, bool create_offscreen = true);

        [[nodiscard]] virtual int get_weight() const;

        [[nodiscard]] virtual tmillis_t get_duration() const;

        [[nodiscard]] virtual nlohmann::json to_json() const;
        [[nodiscard]] virtual string get_name() const = 0;

        virtual void initialize(rgb_matrix::RGBMatrix *matrix);
        virtual void cleanup(rgb_matrix::RGBMatrix *matrix);

        [[nodiscard]] bool is_initialized() const;

        /// Returns true if the scene should continue rendering, false if not
        virtual bool render(rgb_matrix::RGBMatrix *matrix) = 0;

        static Scene *from_json(const nlohmann::json &j);
    };
}