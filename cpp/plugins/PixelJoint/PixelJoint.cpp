#include "PixelJoint.h"
#ifndef PLUGIN_TEST
    #include "scenes/ImageScene.h"
#endif
#include <vector>

using std::vector;

#ifndef PLUGIN_TEST
vector<SceneWrapper *> PixelJoint::get_scenes() {
    return {
        new ImageSceneWrapper()
    };
}

vector<ImageTypeWrapper *> PixelJoint::get_images_types() {
    return {};
}

#else
string PixelJoint::test() {
    return "Yoo pixel joint";
}
#endif

PixelJoint::PixelJoint() = default;

extern "C" PixelJoint *createPixelJoint()
{
    return new PixelJoint();
}

extern "C" void destroyPixelJoint(PixelJoint *c)
{
    delete c;
}
