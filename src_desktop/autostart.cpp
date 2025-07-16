#include "autostart.h"
#include <cstdlib>
#include <fstream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <comdef.h>
#include <wrl/client.h>
#include <shobjidl.h>
#include <system_error>
#endif

namespace Autostart {
#ifdef _WIN32
static std::string lastError;

std::wstring s2ws(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string getStartupFolder() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, NULL, &path))) {
        std::wstring ws(path);
        CoTaskMemFree(path);
        return std::string(ws.begin(), ws.end());
    }
    return "";
}

bool enable(const std::string& exePath, const std::string& appName) {
    std::string shortcutPath = getStartupFolder() + "\\" + appName + ".lnk";
    Microsoft::WRL::ComPtr<IShellLink> pShellLink;
    HRESULT hr = S_OK;
    if (FAILED(CoInitialize(NULL))) {
        lastError = "Failed to initialize COM library: " + std::system_category().message(GetLastError());
        return false;
    }
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pShellLink));
    if (FAILED(hr)) {
        lastError = "Failed to create ShellLink instance: " + std::system_category().message(hr);
        CoUninitialize();
        return false;
    }
    pShellLink->SetPath(s2ws(exePath).c_str());
    pShellLink->SetDescription(s2ws(appName).c_str());
    Microsoft::WRL::ComPtr<IPersistFile> pPersistFile;
    hr = pShellLink.As(&pPersistFile);
    if (FAILED(hr)) {
        lastError = "Failed to get IPersistFile interface: " + std::system_category().message(hr);
        CoUninitialize();
        return false;
    }
    hr = pPersistFile->Save(s2ws(shortcutPath).c_str(), TRUE);
    if (FAILED(hr)) {
        lastError = "Failed to save shortcut file: " + std::system_category().message(hr);
        CoUninitialize();
        return false;
    }
    CoUninitialize();
    lastError.clear();
    return true;
}

bool disable(const std::string& appName) {
    std::string shortcutPath = getStartupFolder() + "\\" + appName + ".lnk";
    std::error_code ec;
    bool result = std::filesystem::remove(shortcutPath, ec);
    if (!result) {
        lastError = "Failed to remove shortcut file: " + ec.message();
    } else {
        lastError.clear();
    }
    return result;
}

std::string getLastError() {
    return lastError;
}

bool isEnabled(const std::string& appName) {
    std::string shortcutPath = getStartupFolder() + "\\" + appName + ".lnk";
    return std::filesystem::exists(shortcutPath);
}

#else // Linux/Unix
static std::string lastError;

std::string getAutostartPath(const std::string& appName) {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    std::string dir = std::string(home) + "/.config/autostart/";
    std::filesystem::create_directories(dir);
    return dir + appName + ".desktop";
}

bool enable(const std::string& exePath, const std::string& appName) {
    std::string desktopFile = getAutostartPath(appName);
    std::ofstream ofs(desktopFile);
    if (!ofs) {
        lastError = "Failed to open autostart file for writing: " + desktopFile + ": " + std::system_category().message(errno);
        return false;
    }
    ofs << "[Desktop Entry]\n";
    ofs << "Type=Application\n";
    ofs << "Exec=" << exePath << "\n";
    ofs << "Hidden=false\n";
    ofs << "NoDisplay=false\n";
    ofs << "X-GNOME-Autostart-enabled=true\n";
    ofs << "Name=" << appName << "\n";
    ofs.close();
    lastError.clear();
    return true;
}

bool disable(const std::string& appName) {
    std::string desktopFile = getAutostartPath(appName);
    std::error_code ec;
    bool result = std::filesystem::remove(desktopFile, ec);
    if (!result) {
        lastError = "Failed to remove autostart file: " + desktopFile + ": " + ec.message();
    } else {
        lastError.clear();
    }
    return result;
}

std::string getLastError() {
    return lastError;
}

bool isEnabled(const std::string& appName) {
    std::string desktopFile = getAutostartPath(appName);
    return std::filesystem::exists(desktopFile);
}
#endif
} // namespace Autostart
