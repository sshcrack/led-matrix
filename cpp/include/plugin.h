#pragma once

#include <vector>
#include <string>
#include "config/image_types/general.h"
#include "led-matrix.h"
#include "matrix_control/scene/Scene.h"

using std::map;

class BasicPlugin {
public:
    virtual map<string, ImageTypes::General*> get_images_types() = 0;
    virtual map<string, Scenes::Scene*> get_scenes(rgb_matrix::RGBMatrix* matrix) = 0;
};