#include "pixelJoint.h"
#include "matrix_control/scene/ImageScene.h"
#include <vector>
#include <string>

using std::vector;
using std::map;

map<string, Scenes::Scene*> PixelJoint::get_scenes(rgb_matrix::RGBMatrix *matrix) {
    return {
        {"pixelJoint", new Scenes::ImageScene(matrix)}
    };
}

map<string, ImageTypes::General*> PixelJoint::get_images_types() {
    return {};
}
