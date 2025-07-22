#include "common.h"

std::mutex currSceneMutex;
std::string currScene;

std::mutex registryMutex;
Server::ws_registry_t registry;