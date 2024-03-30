#pragma once

#include <vector>
#include <string>
#include "../src/config/image_types/general.h"
#include "../src/matrix_control/scene/Scene.h"

using namespace std;

class BasicPlugin {
public:
    virtual vector<map<string, ImageTypes::General>> get_images_types() = 0;
    virtual vector<map<string, Scenes::Scene>> get_scenes() = 0;
};