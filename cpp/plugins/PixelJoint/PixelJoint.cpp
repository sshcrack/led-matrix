#include "PixelJoint.h"
#include "scenes/image/ImageScene.h"
#include "providers/collection.h"
#include "providers/pages.h"

#include <vector>

using std::vector;
using namespace ImageProviders;

vector<SceneWrapper *> PixelJoint::get_scenes() {
    return {
            new ImageSceneWrapper()
    };
}

vector<ImageProviderWrapper *> PixelJoint::get_image_providers() {
    return {
            new CollectionWrapper(),
            new PagesWrapper()
    };
}

PixelJoint::PixelJoint() = default;

extern "C" [[maybe_unused]] PixelJoint *createPixelJoint() {
    return new PixelJoint();
}

extern "C" [[maybe_unused]] void destroyPixelJoint(PixelJoint *c) {
    delete c;
}
