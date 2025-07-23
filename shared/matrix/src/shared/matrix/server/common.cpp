#include "shared/matrix/server/common.h"


namespace Server {
    std::shared_mutex registryMutex;
    ws_registry_t registry;

    std::shared_mutex currSceneMutex;
    std::string currScene;
}