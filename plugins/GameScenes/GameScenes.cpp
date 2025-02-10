#include "GameScenes.h"
#include "shared/utils/shared.h"
#include "spdlog/spdlog.h"
#include "scenes/PingPongGameScene.h"
#include "scenes/tetris/TetrisScene.h"
#include "scenes/MazeGameScene.h"

using namespace Scenes;

extern "C" [[maybe_unused]] GameScenes *createGameScenes() {
    return new GameScenes();
}

extern "C" [[maybe_unused]] void destroyGameScenes(GameScenes *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper>> GameScenes::get_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> GameScenes::get_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper>>();
    scenes.push_back(std::make_unique<PingPongGameSceneWrapper>());
    scenes.push_back(std::make_unique<TetrisSceneWrapper>());
    scenes.push_back(std::make_unique<MazeGameSceneWrapper>());

    return scenes;
}

GameScenes::GameScenes() = default;
