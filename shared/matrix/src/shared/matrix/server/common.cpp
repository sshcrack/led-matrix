#include "shared/matrix/server/common.h"


namespace Server {
    std::shared_mutex registryMutex;
    ws_registry_t registry;
    std::atomic<int> desktop_connection_count{0};

    std::shared_mutex currSceneMutex;
    std::shared_ptr<Scenes::Scene> currScene;
}