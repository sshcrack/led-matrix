#include <iostream>
#include <set>
#include "spdlog/spdlog.h"
#include <filesystem>

#include "shared/desktop/plugin_loader/loader.h"
#include "shared/common/plugin_loader/lib_name.h"
#include "shared/desktop/plugin/main.h"
#include "shared/common/utils/utils.h"

#ifdef _WIN32
#include <windows.h>
#include <libloaderapi.h>
#else
#include <dlfcn.h>
#endif

using namespace spdlog;
namespace fs = std::filesystem;
using Plugins::DesktopPlugin;
using Plugins::PluginManager;

PluginManager::PluginManager() = default;

void PluginManager::destroy_plugins()
{
    info("Destroying plugins...");
    std::flush(std::cout);

    for (const auto &item : loaded_plugins)
    {
        void (*destroy)(DesktopPlugin *);

#ifdef _WIN32
        // Windows version using GetProcAddress
        destroy = (void (*)(DesktopPlugin *))GetProcAddress(
            (HMODULE)item.handle,
            item.destroyFnName.c_str());
#else
        // Linux version using dlsym
        destroy = (void (*)(DesktopPlugin *))dlsym(item.handle, item.destroyFnName.c_str());
#endif

        if (destroy)
        {
            destroy(item.plugin);
        }
        else
        {
            warn("Destroy function '{}' missing for desktop plugin '{}'", item.destroyFnName, item.name);
        }

#ifdef _WIN32
        if (item.handle != nullptr && !FreeLibrary((HMODULE)item.handle))
        {
            warn("Failed to free desktop plugin handle '{}' (error={})", item.name, GetLastError());
        }
#else
        if (item.handle != nullptr && dlclose(item.handle) != 0)
        {
            warn("Failed to close desktop plugin handle '{}': {}", item.name, dlerror());
        }
#endif
    }

    loaded_plugins.clear();
    initialized = false;
}

void PluginManager::delete_references()
{
}

std::vector<std::pair<std::string, DesktopPlugin *>> PluginManager::get_plugins()
{
    std::vector<std::pair<std::string, DesktopPlugin *>> plugins;
    for (const auto &item : loaded_plugins)
    {
        plugins.emplace_back(std::make_pair(item.name, item.plugin));
    }

    return plugins;
}

void PluginManager::initialize()
{
    if (initialized)
        return;

    const fs::path plugin_dir = get_exec_dir().parent_path() / "plugins";
    std::vector<fs::path> libPaths;

    for (const auto &entry : fs::directory_iterator(plugin_dir))
    {
        if (!entry.is_directory())
            continue;
        fs::path plugin_dir_path = entry.path();
#ifdef _WIN32
        fs::path plugin_path = plugin_dir_path / (plugin_dir_path.filename() += ".dll");
#else
        fs::path plugin_path = plugin_dir_path / (std::string("lib") += plugin_dir_path.filename() += ".so");
#endif

        if (!fs::is_regular_file(plugin_path))
            continue;

        fs::path absolute = fs::absolute(plugin_path);
        libPaths.push_back(absolute);
    }

    // Loading libs to memory
#ifdef _WIN32
    auto dllDirCookie = AddDllDirectory(get_exec_dir().wstring().c_str());
    if (dllDirCookie == 0)
    {
        error("Failed to add plugin directory '{}': {}. Trying anyways to load the plugin...", get_exec_dir().string(), GetLastError());
    }
#endif

    for (const fs::path &plPath : libPaths)
    {
        void *dlhandle = nullptr;

#ifdef _WIN32
        auto plDirCookie = AddDllDirectory(plPath.parent_path().wstring().c_str());
        if (plDirCookie == 0)
        {
            error("Failed to add plugin directory '{}': {}. Trying anyways to load the plugin...", plPath.parent_path().string(), GetLastError());
        }

        // Windows implementation
        HMODULE handle = LoadLibraryExW(plPath.wstring().c_str(), nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (handle == nullptr)
        {
            DWORD error_code = GetLastError();
            LPSTR message_buffer = nullptr;
            FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&message_buffer, 0, NULL);

            std::string error_msg = message_buffer ? message_buffer : "Unknown error";
            if (message_buffer)
                LocalFree(message_buffer);

            error("Failed to load plugin '{}': {}", plPath.string(), error_msg);
            continue;
        }

        if (plDirCookie != 0)
            RemoveDllDirectory(plDirCookie);

        dlhandle = (void *)handle;
#else
        // Linux implementation
        dlerror(); // Clear any existing errors
        dlhandle = dlopen(plPath.string().c_str(), RTLD_LAZY);
        if (dlhandle == nullptr)
        {
            error("Failed to load plugin '{}': {}", plPath.string(), dlerror());
            continue;
        }
#endif

        fs::path pl_copy = plPath;
        std::string libName = get_lib_name(pl_copy);
        constexpr const char *createSymbol = "plugin_create";
        constexpr const char *destroySymbol = "plugin_destroy";
        constexpr const char *apiVersionSymbol = "plugin_get_api_version";
        constexpr const char *legacyApiVersionSymbol = "get_api_version";
        std::string cn = "create" + libName;
        std::string dn = "destroy" + libName;

        int (*get_api_version)() = nullptr;
#ifdef _WIN32
        get_api_version = (int(*)())GetProcAddress((HMODULE)dlhandle, apiVersionSymbol);
        if (get_api_version == nullptr)
        {
            get_api_version = (int(*)())GetProcAddress((HMODULE)dlhandle, legacyApiVersionSymbol);
        }
#else
        dlerror();
        get_api_version = (int (*)())(dlsym(dlhandle, apiVersionSymbol));
        const char *api_error = dlerror();
        if (api_error != nullptr || get_api_version == nullptr)
        {
            dlerror();
            get_api_version = (int (*)())(dlsym(dlhandle, legacyApiVersionSymbol));
            api_error = dlerror();
        }
#endif

        if (get_api_version == nullptr)
        {
            error("Plugin '{}' does not export desktop API version symbol ('{}' or '{}')", plPath.string(), apiVersionSymbol, legacyApiVersionSymbol);
#ifdef _WIN32
            FreeLibrary((HMODULE)dlhandle);
#else
            dlclose(dlhandle);
#endif
            continue;
        }

        const int api_version = get_api_version();
        if (api_version != DESKTOP_PLUGIN_API_VERSION)
        {
            error("Plugin '{}' has incompatible desktop API version {} (expected {})", plPath.string(), api_version, DESKTOP_PLUGIN_API_VERSION);
#ifdef _WIN32
            FreeLibrary((HMODULE)dlhandle);
#else
            dlclose(dlhandle);
#endif
            continue;
        }

        // Get create function
        DesktopPlugin *(*create)() = nullptr;

#ifdef _WIN32
        create = (DesktopPlugin * (*)())(GetProcAddress((HMODULE)dlhandle, createSymbol));
        if (create == nullptr)
        {
            create = (DesktopPlugin * (*)())(GetProcAddress((HMODULE)dlhandle, cn.c_str()));
            if (create == nullptr)
            {
                DWORD error_code = GetLastError();
                error("Symbol lookup error in plugin '{}': Error code {}", plPath.string(), error_code);
                error("Expected symbol '{}' or '{}' not found", createSymbol, cn);
                FreeLibrary((HMODULE)dlhandle);
                continue;
            }
        }
#else
        dlerror(); // Clear any existing errors before dlsym
        create = (DesktopPlugin * (*)())(dlsym(dlhandle, createSymbol));
        const char *dlsym_error = dlerror();

        if (dlsym_error != nullptr || create == nullptr)
        {
            dlerror();
            create = (DesktopPlugin * (*)())(dlsym(dlhandle, cn.c_str()));
            dlsym_error = dlerror();

            if (dlsym_error != nullptr || create == nullptr)
            {
                error("Symbol lookup error in plugin '{}': {}", plPath.string(), dlsym_error != nullptr ? dlsym_error : "unknown");
                error("Expected symbol '{}' or '{}' not found", createSymbol, cn);
                dlclose(dlhandle);
                continue;
            }
        }
#endif

        // Verify destroy function exists before creating plugin
        void *destroy_sym = nullptr;
        std::string resolvedDestroySymbol = destroySymbol;

#ifdef _WIN32
        destroy_sym = (void *)GetProcAddress((HMODULE)dlhandle, destroySymbol);
        if (destroy_sym == nullptr)
        {
            destroy_sym = (void *)GetProcAddress((HMODULE)dlhandle, dn.c_str());
            resolvedDestroySymbol = dn;
            if (destroy_sym == nullptr)
            {
                error("Destroy function '{}' or '{}' not found in plugin '{}'", destroySymbol, dn, plPath.string());
                FreeLibrary((HMODULE)dlhandle);
                continue;
            }
        }
#else
        dlerror();
        destroy_sym = dlsym(dlhandle, destroySymbol);
        const char *destroy_error = dlerror();
        if (destroy_error != nullptr || destroy_sym == nullptr)
        {
            dlerror();
            destroy_sym = dlsym(dlhandle, dn.c_str());
            destroy_error = dlerror();
            resolvedDestroySymbol = dn;
            if (destroy_error != nullptr || destroy_sym == nullptr)
            {
                error("Destroy function '{}' or '{}' not found in plugin '{}'", destroySymbol, dn, plPath.string());
                dlclose(dlhandle);
                continue;
            }
        }
#endif

        try
        {
            DesktopPlugin *p = create();

            // Get plugin location (platform-specific)
            fs::path plugin_location = plPath; // Default fallback

#ifdef _WIN32
            // On Windows, we already have the filename
            p->_plugin_location = plugin_location;
#else
            // On Linux, use dladdr to get more precise location info
            Dl_info dl_info;
            if (dladdr((void *)create, &dl_info) != 0)
            {
                p->_plugin_location = dl_info.dli_fname;
            }
            else
            {
                p->_plugin_location = plugin_location;
            }
#endif

            trace("Successfully loaded plugin {}", plPath.string());

            PluginInfo info = {
                .handle = dlhandle,
                .destroyFnName = resolvedDestroySymbol,
                .name = libName,
                .plugin = p,
            };
            loaded_plugins.emplace_back(info);
        }
        catch (const std::exception &e)
        {
            error("Failed to initialize plugin '{}': {}", plPath.string(), e.what());

#ifdef _WIN32
            FreeLibrary((HMODULE)dlhandle);
#else
            dlclose(dlhandle);
#endif
        }
    }

#ifdef _WIN32
    if (dllDirCookie != 0)
        RemoveDllDirectory(dllDirCookie);
#endif

    info("Loaded a total of {} plugins.", loaded_plugins.size());

    initialized = true;
}

PluginManager *PluginManager::instance()
{
    static PluginManager instance;
    return &instance;
}
