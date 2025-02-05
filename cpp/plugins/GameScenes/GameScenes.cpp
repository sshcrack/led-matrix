#include "GameScenes.h"
#include "shared/utils/shared.h"
#include "spdlog/spdlog.h"
#include "scenes/PingPongGameScene.h"

using namespace Scenes;

extern "C" [[maybe_unused]] GameScenes *createGameScenes() {
    return new GameScenes();
}

extern "C" [[maybe_unused]] void destroyGameScenes(GameScenes *c) {
    delete c;
}

vector<ImageProviderWrapper *> GameScenes::get_image_providers() {
    return {};
}

vector<SceneWrapper *> GameScenes::get_scenes() {
    return {
        new PingPongGameSceneWrapper()
    };
}

GameScenes::GameScenes() = default;
