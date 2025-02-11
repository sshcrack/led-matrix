#include "PixelJoint.h"
#include "scenes/image/ImageScene.h"
#include "providers/collection.h"
#include "providers/pages.h"

#include <vector>

using std::vector;
using namespace ImageProviders;

vector<std::unique_ptr<ImageProviderWrapper, void (*)(Plugins::ImageProviderWrapper *)>>
PixelJoint::create_image_providers() {
    auto scenes = vector<std::unique_ptr<ImageProviderWrapper, void (*)(Plugins::ImageProviderWrapper *)>>();
    auto deleteWrapper = [](Plugins::ImageProviderWrapper *scene) {
        delete scene;
    };

    scenes.push_back({new CollectionWrapper(), deleteWrapper});
    scenes.push_back({new PagesWrapper(), deleteWrapper});

    return scenes;
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> PixelJoint::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>>();
    scenes.push_back({new ImageSceneWrapper(), [](Plugins::SceneWrapper *scene) {
        delete scene;
    }});

    return scenes;
}

PixelJoint::PixelJoint() = default;

extern "C" [[maybe_unused]] PixelJoint *createPixelJoint() {
    return new PixelJoint();
}

extern "C" [[maybe_unused]] void destroyPixelJoint(PixelJoint *c) {
    delete c;
}
