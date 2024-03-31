#pragma once
#include "plugin.h"

class PixelJoint : BasicPlugin {
    map<string, ImageTypes::General*> get_images_types() override;
    map<string, Scenes::Scene*> get_scenes(rgb_matrix::RGBMatrix *matrix) override;
};
