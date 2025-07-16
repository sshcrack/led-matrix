#pragma once
#include <string>
#include <functional>

class SingleInstanceManager {
public:
    // Callback is called when a focus request is received
    SingleInstanceManager(const std::string& appId, std::function<void()> focusCallback);
    ~SingleInstanceManager();
    bool isPrimaryInstance() const;
    // Call this periodically in your main loop (Linux only)
    void poll();
private:
    bool _isPrimary = false;
    std::string _appId;
    std::function<void()> _focusCallback;
#ifdef _WIN32
    void* _mutex = nullptr;
    void* _window = nullptr;
    void createHiddenWindow();
    static long CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#else
    // DBus connection objects, etc.
    void* _dbusConn = nullptr;
    void setupDbus();
    void sendFocusRequest();
#endif
};
