#pragma once
#include <GLFW/glfw3.h>

void initializeGLFW(const char*& glsl_version);
GLFWwindow* createWindow(float scale);
void setupImGui(GLFWwindow* window, const char* glsl_version, float scale);
