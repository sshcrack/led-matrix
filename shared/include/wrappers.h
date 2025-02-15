#pragma once

#include <nlohmann/json.hpp>
#include "Scene.h"
#include "shared/config/image_providers/general.h"
#include "shared/utils/utils.h"

namespace Plugins {
    class ImageProviderWrapper {
        string _cachedName;

    public:
        virtual ~ImageProviderWrapper() = default;

        virtual std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> create_default() = 0;

        virtual std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> from_json(
            const nlohmann::json &json) = 0;

        virtual string get_name() {
            if (_cachedName.empty())
                _cachedName = create_default()->get_name();

            return _cachedName;
        }
    };

    class SceneWrapper {
        std::shared_ptr<Scenes::Scene> default_scene;

    public:
        virtual std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() = 0;

        virtual ~SceneWrapper() = default;

        virtual string get_name() {
            return get_default()->get_name();
        }

        std::shared_ptr<Scenes::Scene> get_default() {
            if (default_scene == nullptr) {
                default_scene = create();
                default_scene->register_properties();
            }

            return default_scene;
        }
    };
}
