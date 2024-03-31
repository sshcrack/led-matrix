#include <dlfcn.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>

#include "loader.h"
#include "interrupt.h"
#include "lib_name.h"
#include "lib_glob.h"
#include "plugin.h"


std::set<std::string> libNames;
std::mutex libLock;

void pluginMainloop(unsigned int update_s) {
    const auto naptime = std::chrono::milliseconds(100);

    while (!interrupt_received) {
        for (std::string pl_name: libNames) {
            void *dlhandle = dlopen(pl_name.c_str(), RTLD_LAZY);

            std::pair<std::string, std::string> delibbed =
                    PluginLoader::get_lib_name(pl_name);

            BasicPlugin *(*create)();
            void (*destroy)(BasicPlugin *);

            std::string cn = "create" + delibbed.second;
            std::string dn = "destroy" + delibbed.second;

            create = (BasicPlugin *(*)()) dlsym(dlhandle, cn.c_str());
            destroy = (void (*)(BasicPlugin *)) dlsym(dlhandle, dn.c_str());

            BasicPlugin *p = create();

            std::cout << "invoking " << pl_name << "get image types";
            p->get_images_types();

            destroy(p);
        }

        unsigned int napped = 0;
        while (!interrupt_received) {
            // spin here too, so we can check running status multiple
            // times during a long sleep
            if (napped > update_s * 1000) {
                break;
            }
            std::this_thread::sleep_for(naptime);
            napped += 100;
        }
    }

    std::cout << "Exiting pluginMainloop thread" << std::endl;
}

void updateLibs(unsigned int update_s) {
    const auto naptime = std::chrono::milliseconds(100);

    while (!interrupt_received) {
        std::cout << "Checking for new libs" << std::endl;

#if __APPLE__
        const std::string libGlob("plugins/*.dylib");
#else
        const std::string libGlob("plugins/*.so");
#endif

        std::vector<std::string> filenames = PluginLoader::lib_glob(libGlob);

        size_t before = libNames.size();
        libLock.lock(); // get locked, boi ------------------------------//
        for (const std::string &p_name: filenames) {                            //
            libNames.insert(p_name);                                     //
        }                                                                //
        libLock.unlock(); // got 'em ------------------------------------//
        size_t after = libNames.size();

        if (after - before > 0) {
            std::cout << "Found " << after - before << " new plugins";
            std::cout << std::endl;
        }

        unsigned int napped = 0;
        while (!interrupt_received) {
            // spin here too, so we can check interrupt_received status multiple
            // times during a long sleep
            if (napped > update_s * 1000) {
                break;
            }
            std::this_thread::sleep_for(naptime);
            napped += 100;
        }
    }

    std::cout << "Exiting update thread" << std::endl;
}


std::pair<std::thread, std::thread> PluginLoader::initialize() {
    std::thread updateThread(updateLibs, 5);
    std::thread mainloopThread(pluginMainloop, 5);

    return { std::move(updateThread), std::move(mainloopThread) };
}
