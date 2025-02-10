#include "GeneralSceneDeleter.h"
#include <iostream>

namespace AmbientScenes {
    void deleteScene(Scenes::Scene* scene) {
        std::cout << "Deleting scene" << std::endl << std::flush;
        delete scene;
    }
}