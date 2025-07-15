#include <GLFW/glfw3.h>
#include <iostream>

void minimizeToToolbar(GLFWwindow* window) {
    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) == 0) {
        glfwIconifyWindow(window);
        std::cout << "Window minimized to toolbar." << std::endl;
    }
}
