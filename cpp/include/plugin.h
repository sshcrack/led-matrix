#pragma once

#include <vector>
#include <string>

#ifndef PLUGIN_TEST
#include "wrappers.h"
#endif
using std::vector;
using std::string;

namespace Plugins {
    class BasicPlugin {
    public:
#ifndef PLUGIN_TEST
        virtual vector<ImageTypeWrapper*> get_images_types() = 0;
        virtual vector<SceneWrapper*> get_scenes() = 0;
#else

        virtual string test() = 0;

#endif
    };
}