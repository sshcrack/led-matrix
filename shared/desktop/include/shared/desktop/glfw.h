#pragma once
#include <GLFW/glfw3.h>
#include "shared/desktop/macro.h"

SHARED_DESKTOP_API void setMainGLFWWindow(GLFWwindow* window);
SHARED_DESKTOP_API GLFWwindow* getMainGLFWWindow();