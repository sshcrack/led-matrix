#include <dlfcn.h>

#include <set>
#include <spdlog/spdlog.h>

#include "loader.h"
#include "lib_name.h"
#include "lib_glob.h"
#include "plugin.h"

using namespace spdlog;
using Plugins::BasicPlugin;

Plugins::PluginManager::PluginManager() {
    const std::string libGlob("plugins/*.so");

    std::vector<std::string> filenames = Plugins::lib_glob(libGlob);

    std::set<std::string> libNames;
    for (const std::string &p_name: filenames) {
        libNames.insert(p_name);
    }


    // Loading libs to memory

    for (std::string pl_name: libNames) {
        void *dlhandle = dlopen(pl_name.c_str(), RTLD_LAZY | RTLD_GLOBAL);

        std::pair<std::string, std::string> delibbed =
                Plugins::get_lib_name(pl_name);

        BasicPlugin *(*create)();

        std::string cn = "create" + delibbed.second;
        std::string dn = "destroy" + delibbed.second;

        std::cout << "CreateFunc: '" << cn << "' Destroy: '" << dn << "'" << std::endl;
        flush(std::cout);
        create = (BasicPlugin *(*)()) dlsym(dlhandle, cn.c_str());
        if(create == nullptr) {
            error("Could not find symbol '{}' for Plugin '{}'. Output: {}", cn, pl_name, dlerror());
            std::exit(-1);
        }

        BasicPlugin *p = create();

        info("Loaded plugin {}", pl_name);
        loaded_plugins.emplace_back(dlhandle, dn, p);
    }
}

void Plugins::PluginManager::terminate() {
    info("Destroying plugins...");
    for (const auto &item: loaded_plugins) {
        void (*destroy)(BasicPlugin *);
        destroy = (void (*)(BasicPlugin *)) dlsym(get<0>(item), get<1>(item).c_str());

        destroy(get<2>(item));
    }
}


std::vector<Plugins::BasicPlugin *> Plugins::PluginManager::get_plugins() {
    std::vector<Plugins::BasicPlugin*> plugins;
    for (const auto &item: loaded_plugins) {
        plugins.emplace_back(get<2>(item));
    }

    return plugins;
}


#ifndef PLUGIN_TEST
std::vector<Plugins::SceneWrapper*> Plugins::PluginManager::get_scenes() {
    std::vector<Plugins::SceneWrapper*> scenes;

    for (const auto &item: get_plugins()) {
        scenes.insert(scenes.end(), item->get_scenes().begin(), item->get_scenes().end());
    }

    return scenes;
}


std::vector<Plugins::ImageTypeWrapper *> Plugins::PluginManager::get_image_type() {
    std::vector<Plugins::ImageTypeWrapper*> types;
    for (const auto &item: get_plugins()) {
        types.insert(types.end(), item->get_images_types().begin(), item->get_images_types().end());
    }

    return types;
}

#endif
