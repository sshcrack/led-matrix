#pragma once
#include <nlohmann/json.hpp>
#include "led-matrix.h"
#include "Scene.h"
#include "config/image_providers/general.h"

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