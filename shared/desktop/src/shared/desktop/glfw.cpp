#include "shared/desktop/glfw.h"

static GLFWwindow* g_mainWindow = nullptr;

void setMainGLFWWindow(GLFWwindow* window) {
    g_mainWindow = window;
}

GLFWwindow* getMainGLFWWindow() {
    return g_mainWindow;
}
