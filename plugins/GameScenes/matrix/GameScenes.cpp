#include "GameScenes.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"
#include "scenes/PingPongGameScene.h"
#include "scenes/tetris/TetrisScene.h"
#include "scenes/MazeGameScene.h"

using namespace Scenes;

extern "C" PLUGIN_EXPORT GameScenes *createGameScenes() {
    return new GameScenes();
}

extern "C" PLUGIN_EXPORT void destroyGameScenes(GameScenes *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>> GameScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> GameScenes::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>>();
    auto destroyScene = [](SceneWrapper *scene) {
        delete scene;
    };

    scenes.push_back({new PingPongGameSceneWrapper(), destroyScene});
    scenes.push_back({new TetrisSceneWrapper(), destroyScene});
    scenes.push_back({new MazeGameSceneWrapper(), destroyScene});

    return scenes;
}

GameScenes::GameScenes() = default;
