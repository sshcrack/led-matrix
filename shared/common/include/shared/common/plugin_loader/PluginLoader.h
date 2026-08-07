#pragma once

#include <vector>
#include <algorithm>
#include <string>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "spdlog/spdlog.h"
#include "shared/common/plugin_loader/lib_name.h"
#include "shared/common/utils/utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Plugins {

template<typename PluginBase>
class PluginLoader {
protected:
    struct Data {
        void* handle;
        std::string destroyFnName;
        std::string name;
        PluginBase* plugin;
    };

    std::vector<Data> loaded_plugins;
    bool initialized = false;

    explicit PluginLoader(std::string fhs_dir_name, int dlopen_flags = RTLD_LAZY)
        : fhs_plugin_dir_name_(std::move(fhs_dir_name))
        , dlopen_flags_(dlopen_flags)
    {}

public:
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;
    virtual ~PluginLoader() = default;

    void initialize() {
        if (initialized)
            return;

        auto exec_dir = get_exec_dir();
        auto raw_plugin = getenv("PLUGIN_DIR");

        std::filesystem::path plugin_dir;
        if (raw_plugin != nullptr) {
            plugin_dir = std::filesystem::path(raw_plugin);
        } else if (std::filesystem::is_directory(exec_dir / "plugins")) {
            plugin_dir = exec_dir / "plugins";
        } else {
            auto fhs = exec_dir.parent_path() / "lib" / fhs_plugin_dir_name_ / "plugins";
            plugin_dir = std::filesystem::is_directory(fhs) ? fhs : exec_dir.parent_path() / "plugins";
        }

        if (!std::filesystem::is_directory(plugin_dir)) {
            spdlog::warn("Plugin directory '{}' does not exist", plugin_dir.string());
            initialized = true;
            return;
        }

        std::vector<std::filesystem::path> libPaths;
        for (const auto& entry : std::filesystem::directory_iterator(plugin_dir)) {
            if (!entry.is_directory())
                continue;
            auto plugin_dir_path = entry.path();
#ifdef _WIN32
            auto plugin_path = plugin_dir_path / (plugin_dir_path.filename() += ".dll");
#else
            auto plugin_path = plugin_dir_path / (std::string("lib") += plugin_dir_path.filename() += ".so");
#endif
            if (!std::filesystem::is_regular_file(plugin_path))
                continue;
            libPaths.push_back(std::filesystem::absolute(plugin_path));
        }
        std::sort(libPaths.begin(), libPaths.end());

#ifdef _WIN32
        auto dllDirCookie = AddDllDirectory(get_exec_dir().wstring().c_str());
        if (dllDirCookie == 0) {
            spdlog::error("Failed to add plugin directory '{}': {}",
                get_exec_dir().string(), GetLastError());
        }
#endif

        for (const auto& plPath : libPaths) {
            void* dlhandle = nullptr;

#ifdef _WIN32
            auto plDirCookie = AddDllDirectory(plPath.parent_path().wstring().c_str());
            if (plDirCookie == 0) {
                spdlog::error("Failed to add plugin directory '{}': {}. Trying anyways...",
                    plPath.parent_path().string(), GetLastError());
            }

            HMODULE handle = LoadLibraryExW(plPath.wstring().c_str(), nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (handle == nullptr) {
                DWORD error_code = GetLastError();
                LPSTR message_buffer = nullptr;
                FormatMessageA(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPSTR)&message_buffer, 0, NULL);
                std::string error_msg = message_buffer ? message_buffer : "Unknown error";
                if (message_buffer)
                    LocalFree(message_buffer);
                spdlog::error("Failed to load plugin '{}': {}", plPath.string(), error_msg);
                if (plDirCookie != 0)
                    RemoveDllDirectory(plDirCookie);
                continue;
            }

            if (plDirCookie != 0)
                RemoveDllDirectory(plDirCookie);
            dlhandle = (void*)handle;
#else
            dlerror();
            dlhandle = dlopen(plPath.c_str(), dlopen_flags_);
            if (dlhandle == nullptr) {
                spdlog::error("Failed to load plugin '{}': {}", plPath.string(), dlerror());
                continue;
            }
#endif

            std::string libName = get_lib_name(plPath);
            std::string cn = "create" + libName;
            std::string dn = "destroy" + libName;

            PluginBase* (*create)() = nullptr;
            void* destroy_sym = nullptr;

#ifdef _WIN32
            create = (PluginBase* (*)())(GetProcAddress((HMODULE)dlhandle, cn.c_str()));
            if (create == nullptr) {
                DWORD error_code = GetLastError();
                spdlog::error("Symbol lookup error in plugin '{}': Error code {}", plPath.string(), error_code);
                spdlog::error("Expected symbol '{}' not found", cn);
                FreeLibrary((HMODULE)dlhandle);
                continue;
            }

            destroy_sym = (void*)GetProcAddress((HMODULE)dlhandle, dn.c_str());
            if (destroy_sym == nullptr) {
                spdlog::error("Destroy function '{}' not found in plugin '{}'", dn, plPath.string());
                FreeLibrary((HMODULE)dlhandle);
                continue;
            }
#else
            dlerror();
            create = (PluginBase* (*)())(dlsym(dlhandle, cn.c_str()));
            const char* dlsym_error = dlerror();
            if (dlsym_error != nullptr) {
                spdlog::error("Symbol lookup error in plugin '{}': {}", plPath.string(), dlsym_error);
                spdlog::error("Expected symbol '{}' not found", cn);
                dlclose(dlhandle);
                continue;
            }

            dlerror();
            destroy_sym = dlsym(dlhandle, dn.c_str());
            if (dlerror() != nullptr || destroy_sym == nullptr) {
                spdlog::error("Destroy function '{}' not found in plugin '{}'", dn, plPath.string());
                dlclose(dlhandle);
                continue;
            }
#endif

            try {
                PluginBase* p = create();
                if (p == nullptr)
                    throw std::runtime_error("plugin factory returned null");

#ifdef _WIN32
                p->_plugin_location = plPath.string();
#else
                Dl_info dl_info;
                if (dladdr((void*)create, &dl_info) != 0)
                    p->_plugin_location = dl_info.dli_fname;
                else
                    p->_plugin_location = plPath.string();
#endif

                spdlog::trace("Successfully loaded plugin {}", plPath.string());

                Data info = {
                    .handle = dlhandle,
                    .destroyFnName = dn,
                    .name = libName,
                    .plugin = p,
                };
                loaded_plugins.emplace_back(info);
            } catch (const std::exception& e) {
                spdlog::error("Failed to initialize plugin '{}': {}", plPath.string(), e.what());
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

        spdlog::info("Loaded a total of {} plugins.", loaded_plugins.size());
        initialized = true;
    }

    void destroy_plugins() {
        if (loaded_plugins.empty()) {
            initialized = false;
            return;
        }

        spdlog::info("Destroying {} plugins...", loaded_plugins.size());
        std::flush(std::cout);

        // Reverse load order mirrors normal C++ object teardown and prevents
        // dependencies loaded later from disappearing underneath earlier ones.
        for (auto it = loaded_plugins.rbegin(); it != loaded_plugins.rend(); ++it) {
            auto &item = *it;
            using DestroyFn = void (*)(PluginBase*);
            DestroyFn destroy = nullptr;

#ifdef _WIN32
            destroy = (DestroyFn)GetProcAddress((HMODULE)item.handle, item.destroyFnName.c_str());
#else
            dlerror();
            destroy = (DestroyFn)dlsym(item.handle, item.destroyFnName.c_str());
#endif

            if (destroy && item.plugin) {
                try {
                    destroy(item.plugin);
                } catch (const std::exception &e) {
                    spdlog::error("Plugin '{}' threw while being destroyed: {}", item.name, e.what());
                } catch (...) {
                    spdlog::error("Plugin '{}' threw an unknown exception while being destroyed", item.name);
                }
                item.plugin = nullptr;
            }

#ifdef _WIN32
            if (item.handle) FreeLibrary((HMODULE)item.handle);
#else
            if (item.handle) dlclose(item.handle);
#endif
            item.handle = nullptr;
        }

        loaded_plugins.clear();
        initialized = false;
    }

    // Kept for callers that only need to forget loader bookkeeping. Normal
    // shutdown should call destroy_plugins(); clearing live handles leaks DSOs.
    void delete_references() {
        if (!loaded_plugins.empty())
            spdlog::warn("PluginLoader::delete_references called with live plugins; use destroy_plugins instead");
    }

protected:
    std::string fhs_plugin_dir_name_;
    int dlopen_flags_ = RTLD_LAZY;
};

}
