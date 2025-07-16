#include "single_instance_manager.h"
#include <stdexcept>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <string>
#include <thread>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <dbus/dbus.h>
#endif

SingleInstanceManager::SingleInstanceManager(const std::string& appId, std::function<void()> focusCallback)
    : _appId(appId), _focusCallback(focusCallback)
{
#ifdef _WIN32
    std::string mutexName = "Global\\SingleInstance_" + appId;
    _mutex = CreateMutexA(nullptr, TRUE, mutexName.c_str());
    if (!_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        // Find window and send message to focus
        HWND hwnd = FindWindowA("SingleInstanceWindow", nullptr);
        if (hwnd) {
            PostMessageA(hwnd, WM_USER + 1, 0, 0); // Custom message to focus
        }
        throw std::runtime_error("Another instance is already running.");
    }
    createHiddenWindow();
    _isPrimary = true;
#else
    setupDbus();
#endif
}

SingleInstanceManager::~SingleInstanceManager() {
#ifdef _WIN32
    if (_mutex) {
        ReleaseMutex(static_cast<HANDLE>(_mutex));
        CloseHandle(static_cast<HANDLE>(_mutex));
    }
    // Destroy window if needed
#else
    // Clean up DBus connection and release name
    if (_dbusConn) {
        std::string busName = "org." + _appId + ".SingleInstance";
        DBusError err;
        dbus_error_init(&err);
        dbus_bus_release_name((DBusConnection*)_dbusConn, busName.c_str(), &err);
        dbus_connection_unref((DBusConnection*)_dbusConn);
        _dbusConn = nullptr;
    }
#endif
}

bool SingleInstanceManager::isPrimaryInstance() const {
    return _isPrimary;
}

#ifdef _WIN32
void SingleInstanceManager::createHiddenWindow() {
    // Register and create a hidden window to receive focus messages
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = "SingleInstanceWindow";
    RegisterClassA(&wc);
    _window = CreateWindowA("SingleInstanceWindow", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    SetWindowLongPtrA((HWND)_window, GWLP_USERDATA, (LONG_PTR)this);
}

long CALLBACK SingleInstanceManager::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_USER + 1) {
        auto* self = reinterpret_cast<SingleInstanceManager*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        if (self && self->_focusCallback) self->_focusCallback();
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
#endif

#ifndef _WIN32
void SingleInstanceManager::setupDbus() {
    DBusError err;
    dbus_error_init(&err);
    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn) throw std::runtime_error("Failed to connect to DBus");
    std::string busName = "org." + _appId + ".SingleInstance";
    int ret = dbus_bus_request_name(conn, busName.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        // Already running, send focus request
        sendFocusRequest();
        throw std::runtime_error("Another instance is already running.");
    }
    _dbusConn = conn;
    _isPrimary = true;
}

void SingleInstanceManager::sendFocusRequest() {
    DBusError err;
    dbus_error_init(&err);
    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn) return;
    std::string busName = "org." + _appId + ".SingleInstance";
    std::string objPath = "/org/" + _appId + "/SingleInstance";
    DBusMessage* msg = dbus_message_new_method_call(busName.c_str(), objPath.c_str(), "org.SingleInstance.DBus", "ExecuteCallback");
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &(_appId), DBUS_TYPE_INVALID);
    dbus_connection_send(conn, msg, nullptr);
    dbus_message_unref(msg);
}

void SingleInstanceManager::poll() {
    if (!_isPrimary || !_dbusConn) return;
    dbus_connection_read_write_dispatch((DBusConnection*)_dbusConn, 0);
    // Listen for ExecuteCallback and call _focusCallback
    while (DBusMessage* msg = dbus_connection_pop_message((DBusConnection*)_dbusConn)) {
        if (dbus_message_is_method_call(msg, "org.SingleInstance.DBus", "ExecuteCallback")) {
            if (_focusCallback) _focusCallback();
        }
        dbus_message_unref(msg);
    }
}
#endif
