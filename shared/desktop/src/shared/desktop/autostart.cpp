#include "shared/desktop/autostart.h"
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

namespace Autostart
{
#ifdef _WIN32
    std::wstring s2ws(const std::string &str)
    {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    std::string getStartupFolder()
    {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, NULL, &path)))
        {
            std::wstring ws(path);
            CoTaskMemFree(path);
            return std::string(ws.begin(), ws.end());
        }

        return "";
    }

    std::expected<void, std::string> enable(const std::string &exePath, const std::string &appName)
    {
        std::string shortcutPath = getStartupFolder() + "\\" + appName + ".lnk";
        Microsoft::WRL::ComPtr<IShellLinkW> pShellLink;
        HRESULT hr = S_OK;
        if (FAILED(CoInitialize(NULL)))
        {
            return std::unexpected("Failed to initialize COM library: " + std::system_category().message(GetLastError()));
        }

        hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void **)&pShellLink);
        if (FAILED(hr))
        {
            CoUninitialize();
            return std::unexpected("Failed to create ShellLink instance: " + std::system_category().message(hr));
        }

        // Add --start-minimized flag to the command line
        std::string exeWithArgs = exePath + " --start-minimized";
        pShellLink->SetPath(s2ws(exePath).c_str());
        pShellLink->SetArguments(s2ws("--start-minimized").c_str());
        pShellLink->SetDescription(s2ws(appName).c_str());
        Microsoft::WRL::ComPtr<IPersistFile> pPersistFile;
        hr = pShellLink.As(&pPersistFile);
        if (FAILED(hr))
        {
            CoUninitialize();
            return std::unexpected("Failed to get IPersistFile interface: " + std::system_category().message(hr));
        }
        hr = pPersistFile->Save(s2ws(shortcutPath).c_str(), TRUE);
        if (FAILED(hr))
        {
            CoUninitialize();
            return std::unexpected("Failed to save shortcut file: " + std::system_category().message(hr));
        }
        CoUninitialize();

        return {};
    }

    std::expected<void, std::string> disable(const std::string &appName)
    {
        std::string shortcutPath = getStartupFolder() + "\\" + appName + ".lnk";
        std::error_code ec;
        bool result = std::filesystem::remove(shortcutPath, ec);
        if (!result)
            return std::unexpected("Failed to remove shortcut file: " + ec.message());

        return {};
    }

    bool isEnabled(const std::string &appName)
    {
        std::string shortcutPath = getStartupFolder() + "\\" + appName + ".lnk";
        return std::filesystem::exists(shortcutPath);
    }

#else // Linux/Unix
    static std::string lastError;

    std::string getAutostartPath(const std::string &appName)
    {
        const char *home = std::getenv("HOME");
        if (!home)
            return "";
        std::string dir = std::string(home) + "/.config/autostart/";
        std::filesystem::create_directories(dir);
        return dir + appName + ".desktop";
    }

    std::expected<void, std::string> enable(const std::string &exePath, const std::string &appName)
    {
        std::string desktopFile = getAutostartPath(appName);
        std::ofstream ofs(desktopFile);
        if (!ofs)
        {
            return std::unexpected("Failed to open autostart file for writing: " + desktopFile + ": " + std::system_category().message(errno));
        }
        std::filesystem::path exePathObj(exePath);
        std::filesystem::path exePathDir = exePathObj.parent_path().parent_path(); // Assuming icon is a PNG with the same name

        ofs << "[Desktop Entry]\n";
        ofs << "Type=Application\n";
        // Add --start-minimized flag to the Exec line
        ofs << "Exec=" << exePath << " --start-minimized\n";
        ofs << "Icon=" << (exePathDir / "assets" / "app_settings" / "icon.svg").string() << "\n";
        ofs << "Hidden=false\n";
        ofs << "NoDisplay=false\n";
        ofs << "X-GNOME-Autostart-enabled=true\n";
        ofs << "Name=" << appName << "\n";
        ofs.close();

        return {};
    }

    std::expected<void, std::string> disable(const std::string &appName)
    {
        std::string desktopFile = getAutostartPath(appName);
        std::error_code ec;
        bool result = std::filesystem::remove(desktopFile, ec);
        if (!result)
            return std::unexpected("Failed to remove autostart file: " + desktopFile + ": " + ec.message());

        return {};
    }

    bool isEnabled(const std::string &appName)
    {
        std::string desktopFile = getAutostartPath(appName);
        return std::filesystem::exists(desktopFile);
    }
#endif
} // namespace Autostart
