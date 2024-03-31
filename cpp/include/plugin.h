#pragma once

#include <vector>
#include <string>
#include "wrappers.h"

using std::vector;
using std::string;

namespace Plugins {
    class BasicPlugin {
    public:
        virtual vector<ImageTypeWrapper*> get_images_types() = 0;
        virtual vector<SceneWrapper*> get_scenes() = 0;
    };
}