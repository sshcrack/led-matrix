#include "PixelJoint.h"
#include "scenes/image/ImageScene.h"

#include <vector>

using std::vector;

vector<SceneWrapper *> PixelJoint::get_scenes() {
    return {
        new ImageSceneWrapper()
    };
}

vector<ImageTypeWrapper *> PixelJoint::get_images_types() {
    return {};
}

PixelJoint::PixelJoint() = default;

extern "C" PixelJoint *createPixelJoint() {
    return new PixelJoint();
}

extern "C" void destroyPixelJoint(PixelJoint *c) {
    delete c;
}
