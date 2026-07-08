#include "GameScenes.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"
#include "scenes/PingPongGameScene.h"
#include "scenes/tetris/TetrisScene.h"
#include "scenes/MazeGameScene.h"
#include "scenes/SnakeGameScene.h"

using namespace Scenes;

REGISTER_PLUGIN(GameScenes, GameScenes)

vector<std::unique_ptr<ImageProviderWrapper>> GameScenes::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> GameScenes::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> scenes;
    scenes.push_back(std::make_unique<PingPongGameSceneWrapper>());
    scenes.push_back(std::make_unique<TetrisSceneWrapper>());
    scenes.push_back(std::make_unique<MazeGameSceneWrapper>());
    scenes.push_back(std::make_unique<SnakeGameSceneWrapper>());
    return scenes;
}

GameScenes::GameScenes() = default;
