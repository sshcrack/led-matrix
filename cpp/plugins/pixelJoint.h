#pragma once
#include "plugin.h"

class PixelJoint : BasicPlugin {
    vector<map<string, ImageTypes::General>> get_images_types() override;
    vector<map<string, Scenes::Scene>> get_scenes() override;
};
