#pragma once

#include <nlohmann/json.hpp>
#include "led-matrix.h"
#include "Scene.h"
#include "shared/config/image_providers/general.h"
#include "shared/utils/utils.h"

namespace Plugins {
    class ImageProviderWrapper {
    private:
        string _cachedName;
    public:
        virtual ImageProviders::General *create_default() = 0;

        virtual ImageProviders::General *from_json(const nlohmann::json &json) = 0;

        virtual string get_name() {
            if (_cachedName.empty())
                _cachedName = create_default()->get_name();

            return _cachedName;
        }
    };

    class SceneWrapper {
    private:
        string _cachedName;
    public:
        virtual string get_name() {
            if (_cachedName.empty())
                _cachedName = create_default()->get_name();

            return _cachedName;
        }

        virtual Scenes::Scene *create_default() = 0;

        virtual Scenes::Scene *from_json(const nlohmann::json &args) = 0;
    };
}