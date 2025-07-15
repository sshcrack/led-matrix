#include <GLFW/glfw3.h>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#if defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3native.h>
void minimizeToTray(GLFWwindow *window)
{
//REVIEW this was AI generated, check if it works
#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(window);
    // Remove from taskbar: remove WS_EX_APPWINDOW, add WS_EX_TOOLWINDOW
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~WS_EX_APPWINDOW;
    exStyle |= WS_EX_TOOLWINDOW;
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    ShowWindow(hwnd, SW_HIDE); // Hide and show to update style
    ShowWindow(hwnd, SW_SHOW);
    std::cout << "Window hidden from taskbar (Windows)." << std::endl;
#elif defined(__linux__)
    Display *display = glfwGetX11Display();
    Window xwindow = glfwGetX11Window(window);
    Atom state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom skipTaskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    XEvent e = {0};
    e.xclient.type = ClientMessage;
    e.xclient.serial = 0;
    e.xclient.send_event = True;
    e.xclient.window = xwindow;
    e.xclient.message_type = state;
    e.xclient.format = 32;
    e.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    e.xclient.data.l[1] = skipTaskbar;
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = 0;
    e.xclient.data.l[4] = 0;
    XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XFlush(display);
    std::cout << "Window hidden from taskbar (Linux/X11)." << std::endl;

#endif
    // Fallback: just hide the window
    if (glfwGetWindowAttrib(window, GLFW_VISIBLE) != 0)
    {
        glfwHideWindow(window);
        std::cout << "Window hidden (removed from taskbar)." << std::endl;
    }
}

void restoreFromTray(GLFWwindow *window)
{
//REVIEW this was AI generated, check if it works
#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(window);
    // Restore to taskbar: add WS_EX_APPWINDOW, remove WS_EX_TOOLWINDOW
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_APPWINDOW;
    exStyle &= ~WS_EX_TOOLWINDOW;
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    ShowWindow(hwnd, SW_HIDE); // Hide and show to update style
    ShowWindow(hwnd, SW_SHOW);
    std::cout << "Window restored to taskbar (Windows)." << std::endl;
#elif defined(__linux__)
    Display *display = glfwGetX11Display();
    Window xwindow = glfwGetX11Window(window);
    Atom state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom skipTaskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    XEvent e = {0};
    e.xclient.type = ClientMessage;
    e.xclient.serial = 0;
    e.xclient.send_event = True;
    e.xclient.window = xwindow;
    e.xclient.message_type = state;
    e.xclient.format = 32;
    e.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
    e.xclient.data.l[1] = skipTaskbar;
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = 0;
    e.xclient.data.l[4] = 0;
    XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XFlush(display);
    std::cout << "Window restored to taskbar (Linux/X11)." << std::endl;
#endif
    if (glfwGetWindowAttrib(window, GLFW_VISIBLE) == 0)
    {
        glfwShowWindow(window);
        glfwFocusWindow(window); // Bring to front
        std::cout << "Window shown (restored to taskbar)." << std::endl;
    }
}