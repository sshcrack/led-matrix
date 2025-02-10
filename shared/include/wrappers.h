#pragma once

#include <nlohmann/json.hpp>
#include "led-matrix.h"
#include "Scene.h"
#include "shared/config/image_providers/general.h"
#include "shared/utils/utils.h"
#include <iostream>

namespace Plugins {
    class ImageProviderWrapper {
    private:
        string _cachedName;
    public:
        virtual std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> create_default() = 0;

        virtual std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
        from_json(const nlohmann::json &json) = 0;

        virtual string get_name() {
            if (_cachedName.empty())
                _cachedName = create_default()->get_name();

            return _cachedName;
        }
    };

    class SceneWrapper {
    protected:
        Scenes::Scene *default_scene;
        std::vector<Scenes::Scene *> _scenes;
    public:
        virtual Scenes::Scene *create() = 0;

        virtual ~SceneWrapper() = 0;

        virtual string get_name() {
            return get_default()->get_name();
        }

        Scenes::Scene *get_default() {
            if (default_scene == nullptr) {
                default_scene = create();
                default_scene->register_properties();
            }

            return default_scene;
        }
    };
}