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

    std::string ws2s(const std::wstring &wstr)
    {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    std::string getStartupFolder()
    {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, NULL, &path)))
        {
            std::wstring ws(path);
            CoTaskMemFree(path);
            return ws2s(ws);
        }

        return "";
    }

    std::expected<void, std::string> create_shortcut(const std::filesystem::path &lnkPath,
                                                     const std::filesystem::path &target,
                                                     const std::string &arguments,
                                                     const std::string &description)
    {
        const HRESULT init = CoInitialize(NULL);
        if (FAILED(init))
            return std::unexpected("Failed to initialize COM library: " + std::system_category().message(init));

        std::expected<void, std::string> result{};
        {
            Microsoft::WRL::ComPtr<IShellLinkW> pShellLink;
            HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void **)&pShellLink);
            Microsoft::WRL::ComPtr<IPersistFile> pPersistFile;
            if (FAILED(hr))
                result = std::unexpected("Failed to create ShellLink instance: " + std::system_category().message(hr));
            else
            {
                pShellLink->SetPath(target.wstring().c_str());
                pShellLink->SetArguments(s2ws(arguments).c_str());
                pShellLink->SetDescription(s2ws(description).c_str());
                pShellLink->SetWorkingDirectory(target.parent_path().wstring().c_str());
                pShellLink->SetIconLocation(target.wstring().c_str(), 0);
                hr = pShellLink.As(&pPersistFile);
                if (FAILED(hr))
                    result = std::unexpected("Failed to get IPersistFile interface: " + std::system_category().message(hr));
                else if (FAILED(hr = pPersistFile->Save(lnkPath.wstring().c_str(), TRUE)))
                    result = std::unexpected("Failed to save shortcut file: " + std::system_category().message(hr));
            }
        }
        CoUninitialize();
        return result;
    }

    std::expected<void, std::string> enable(const std::string &exePath, const std::string &appName)
    {
        return create_shortcut(getStartupFolder() + "\\" + appName + ".lnk", exePath, "--start-minimized", appName);
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
        ofs << "Exec=\"" << exePath << "\" --start-minimized\n";
        ofs << "Icon=" << (exePathDir / "share" / "icons" / "hicolor" / "scalable" / "apps" / "led-matrix-desktop.svg").string() << "\n";
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
