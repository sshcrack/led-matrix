#pragma once
#include "Scene.h"

#ifdef _WIN32
    #define SCENE_API __declspec(dllexport)
#else
    #define SCENE_API __attribute__((visibility("default")))
#endif

namespace AmbientScenes {
    extern "C" SCENE_API void deleteScene(Scenes::Scene* scene);
}