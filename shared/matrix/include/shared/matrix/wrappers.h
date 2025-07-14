#pragma once

#include "nlohmann/json.hpp"
#include "Scene.h"
#include "shared/config/image_providers/general.h"
#include "shared/utils/utils.h"

namespace Plugins {
    class ImageProviderWrapper {
        std::shared_ptr<ImageProviders::General> default_general;

    public:
        virtual ~ImageProviderWrapper() = default;

        virtual std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> create() = 0;

        virtual string get_name() {
            return get_default()->get_name();
        }

        std::shared_ptr<ImageProviders::General> get_default() {
            if (default_general == nullptr) {
                default_general = create();
                default_general->register_properties();
            }

            return default_general;
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
                default_scene->update_default_properties();
                default_scene->register_properties();
            }

            return default_scene;
        }
    };
}
