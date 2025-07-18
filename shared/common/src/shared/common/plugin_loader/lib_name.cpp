#include "shared/common/plugin_loader/lib_name.h"
#include <filesystem>

std::string Plugins::get_lib_name(std::filesystem::path &p) {
    // hue, https://stackoverflow.com/a/4318543/4765406
    std::string stem = p.stem().string();
    #ifndef _WIN32
        stem = stem.substr(3);
    #endif

    return stem;
}
