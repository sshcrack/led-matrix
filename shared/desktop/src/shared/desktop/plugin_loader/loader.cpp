#ifdef _WIN32
#include <windows.h>
#include <libloaderapi.h>
#else
#include <dlfcn.h>
#endif



#include <iostream>
#include <set>
#include "spdlog/spdlog.h"
#include <filesystem>

#include "shared/desktop/plugin_loader/loader.h"
#include "shared/common/plugin_loader/lib_name.h"
#include "shared/desktop/plugin/main.h"
#include "shared/common/utils/utils.h"

using namespace spdlog;
namespace fs =  std::filesystem;
using Plugins::DesktopPlugin;
using Plugins::PluginManager;

PluginManager::PluginManager() = default;

void PluginManager::destroy_plugins() {
    info("Destroying plugins...");
    std::flush(std::cout);


    for (const auto &item: loaded_plugins) {
        void (*destroy)(DesktopPlugin *);

#ifdef _WIN32
        // Windows version using GetProcAddress
        destroy = (void (*)(DesktopPlugin *)) GetProcAddress(
                (HMODULE)item.handle,
                item.destroyFnName.c_str()
        );
#else
        // Linux version using dlsym
    destroy = (void (*)(BasicPlugin *)) dlsym(item.handle, item.destroyFnName.c_str());
#endif

        if (destroy) {
            destroy(item.plugin);
        }
    }
}

void PluginManager::delete_references() {

}


std::vector<DesktopPlugin *> PluginManager::get_plugins() {
    std::vector<DesktopPlugin *> plugins;
    for (const auto &item: loaded_plugins) {
        plugins.emplace_back(item.plugin);
    }

    return plugins;
}
// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#include <libloaderapi.h>
#else
#include <dlfcn.h>
#endif

void PluginManager::initialize() {
    if (initialized)
        return;

    auto exec_dir = get_exec_dir();
    if (!exec_dir)
        throw std::runtime_error("Could not get executable directory");

    const std::string plugin_dir = "../plugins";
    std::vector<std::string> filenames;

    for (const auto & entry : fs::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file()) {
            if(entry.path().extension() == ".so" || entry.path().extension() == ".dll") {
                filenames.push_back(entry.path().string());
            }
        }
    }

    std::set<std::string> libNames;
    for (const std::string &p_name: filenames) {
        libNames.insert(p_name);
    }

    // Loading libs to memory
    for (const std::string &pl_name: libNames) {
        void *dlhandle = nullptr;

#ifdef _WIN32
        // Windows implementation
        HMODULE handle = LoadLibraryA(pl_name.c_str());
        if (handle == nullptr) {
            DWORD error_code = GetLastError();
            LPSTR message_buffer = nullptr;
            FormatMessageA(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPSTR)&message_buffer, 0, NULL);

            std::string error_msg = message_buffer ? message_buffer : "Unknown error";
            if (message_buffer) LocalFree(message_buffer);

            error("Failed to load plugin '{}': {}", pl_name, error_msg);
            continue;
        }
        dlhandle = (void*)handle;
#else
        // Linux implementation
        dlerror(); // Clear any existing errors
        dlhandle = dlopen(pl_name.c_str(), RTLD_LAZY);
        if (dlhandle == nullptr) {
            error("Failed to load plugin '{}': {}", pl_name, dlerror());
            continue;
        }
#endif

        std::string pl_copy = pl_name;
        std::string delibbed = Plugins::get_lib_name(pl_copy);
        std::string cn = "create" + delibbed;
        std::string dn = "destroy" + delibbed;

        // Get create function
        DesktopPlugin *(*create)() = nullptr;

#ifdef _WIN32
        create = (DesktopPlugin *(*)())(GetProcAddress((HMODULE)dlhandle, cn.c_str()));
        if (create == nullptr) {
            DWORD error_code = GetLastError();
            error("Symbol lookup error in plugin '{}': Error code {}", pl_name, error_code);
            error("Expected symbol '{}' not found", cn);
            FreeLibrary((HMODULE)dlhandle);
            continue;
        }
#else
        dlerror(); // Clear any existing errors before dlsym
        create = (BasicPlugin *(*)())(dlsym(dlhandle, cn.c_str()));
        const char *dlsym_error = dlerror();

        if (dlsym_error != nullptr) {
            error("Symbol lookup error in plugin '{}': {}", pl_name, dlsym_error);
            error("Expected symbol '{}' not found", cn);
            dlclose(dlhandle);
            continue;
        }
#endif

        // Verify destroy function exists before creating plugin
        void *destroy_sym = nullptr;

#ifdef _WIN32
        destroy_sym = (void*)GetProcAddress((HMODULE)dlhandle, dn.c_str());
        if (destroy_sym == nullptr) {
            error("Destroy function '{}' not found in plugin '{}'", dn, pl_name);
            FreeLibrary((HMODULE)dlhandle);
            continue;
        }
#else
        dlerror();
        destroy_sym = dlsym(dlhandle, dn.c_str());
        if (dlerror() != nullptr || destroy_sym == nullptr) {
            error("Destroy function '{}' not found in plugin '{}'", dn, pl_name);
            dlclose(dlhandle);
            continue;
        }
#endif

        try {
            DesktopPlugin *p = create();

            // Get plugin location (platform-specific)
            std::string plugin_location = pl_name; // Default fallback

#ifdef _WIN32
            // On Windows, we already have the filename
            p->_plugin_location = pl_name;
#else
            // On Linux, use dladdr to get more precise location info
            Dl_info dl_info;
            if (dladdr((void*)create, &dl_info) != 0) {
                p->_plugin_location = dl_info.dli_fname;
            } else {
                p->_plugin_location = pl_name;
            }
#endif

            info("Successfully loaded plugin {}", pl_name);

            PluginInfo info = {
                    .handle = dlhandle,
                    .destroyFnName = dn,
                    .plugin = p,
            };
            loaded_plugins.emplace_back(info);
        } catch (const std::exception &e) {
            error("Failed to initialize plugin '{}': {}", pl_name, e.what());

#ifdef _WIN32
            FreeLibrary((HMODULE)dlhandle);
#else
            dlclose(dlhandle);
#endif
        }
    }

    info("Loaded a total of {} plugins.", loaded_plugins.size());
    info("Loading providers to register...");

    initialized = true;
}

PluginManager *PluginManager::instance_ = nullptr;

PluginManager *PluginManager::instance() {
    if (instance_ == nullptr) {
        instance_ = new PluginManager();
    }

    return instance_;
}
