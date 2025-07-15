#pragma once
#include <GLFW/glfw3.h>
#include "shared/desktop/plugin_loader/loader.h"

void renderLoop(GLFWwindow* window, Plugins::PluginManager* instance, ImFont* font);
