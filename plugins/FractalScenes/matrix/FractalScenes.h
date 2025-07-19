#pragma once

#include "shared/matrix/plugin/main.h"

using namespace Plugins;
class FractalScenes final : public BasicPlugin {
public:
    FractalScenes();

    ~FractalScenes() override = default;

    vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>> create_image_providers() override;
    vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> create_scenes() override;
};
