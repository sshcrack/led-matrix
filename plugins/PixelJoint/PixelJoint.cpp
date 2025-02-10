#include "PixelJoint.h"
#include "scenes/image/ImageScene.h"
#include "providers/collection.h"
#include "providers/pages.h"

#include <vector>

using std::vector;
using namespace ImageProviders;

vector<std::unique_ptr<ImageProviderWrapper>> PixelJoint::get_image_providers() {
    auto scenes = vector<std::unique_ptr<ImageProviderWrapper>>();
    scenes.push_back(std::make_unique<CollectionWrapper>());
    scenes.push_back(std::make_unique<PagesWrapper>());

    return scenes;
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> PixelJoint::get_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper>>();
    scenes.push_back(std::make_unique<ImageSceneWrapper>());

    return scenes;
}

PixelJoint::PixelJoint() = default;

extern "C" [[maybe_unused]] PixelJoint *createPixelJoint() {
    return new PixelJoint();
}

extern "C" [[maybe_unused]] void destroyPixelJoint(PixelJoint *c) {
    delete c;
}
