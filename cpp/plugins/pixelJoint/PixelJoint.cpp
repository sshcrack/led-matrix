#include "PixelJoint.h"
#include "scenes/ImageScene.h"
#include <vector>
#include <string>

using std::vector;
using std::map;

map<string, Scenes::Scene*> PixelJoint::get_scenes(rgb_matrix::RGBMatrix *matrix) {
    return {
        {"images", new Scenes::ImageScene(matrix)}
    };
}

map<string, ImageTypes::General*> PixelJoint::get_images_types() {
    return {};
}


extern "C" PixelJoint *createPixelJoint()
{
    return new PixelJoint;
}

extern "C" void destroyPixelJoint(PixelJoint *p)
{
    delete p;
}
