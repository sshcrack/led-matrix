#pragma once

#include <vector>
#include <string>
#include "wrappers.h"

using std::vector;
using std::string;

namespace Plugins {
    class BasicPlugin {
    public:
        virtual vector<Plugins::ImageProviderWrapper *> get_image_providers() = 0;

        virtual vector<Plugins::SceneWrapper *> get_scenes() = 0;
    };
}