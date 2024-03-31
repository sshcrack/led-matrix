#pragma once
#include "config/image_providers/general.h"
#include "led-matrix.h"
#include "matrix_control/scene/Scene.h"
#include <nlohmann/json.hpp>

namespace Plugins {
    class ImageTypeWrapper {
        virtual ImageProviders::General* create_default() = 0;
        virtual ImageProviders::General* from_json(const nlohmann::json& json) = 0;
        virtual string get_name() = 0;
    };

    class SceneWrapper {
        virtual string get_name() = 0;
        virtual Scenes::Scene* create(rgb_matrix::RGBMatrix* matrix) = 0;
    };
}