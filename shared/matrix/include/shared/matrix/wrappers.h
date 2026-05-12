#pragma once

#include "nlohmann/json.hpp"
#include <mutex>
#include "shared/matrix/Scene.h"
#include "shared/matrix/config/image_providers/general.h"
#include "shared/matrix/config/shader_providers/general.h"

namespace Plugins {
    class ImageProviderWrapper {
        std::shared_ptr<ImageProviders::General> default_general;
        std::once_flag default_once;

    public:
        virtual ~ImageProviderWrapper() = default;

        virtual std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)> create() = 0;

        virtual string get_name() {
            return get_default()->get_name();
        }

        std::shared_ptr<ImageProviders::General> get_default() {
            std::call_once(default_once, [this]
            {
                default_general = create();
                default_general->register_properties();
            });

            return default_general;
        }
    };

    class ShaderProviderWrapper {
        std::shared_ptr<ShaderProviders::General> default_general;
        std::once_flag default_once;

    public:
        virtual ~ShaderProviderWrapper() = default;

        virtual std::unique_ptr<ShaderProviders::General, void (*)(ShaderProviders::General *)> create() = 0;

        virtual string get_name() {
            return get_default()->get_name();
        }

        std::shared_ptr<ShaderProviders::General> get_default() {
            std::call_once(default_once, [this]
            {
                default_general = create();
                default_general->register_properties();
            });

            return default_general;
        }
    };

    class SceneWrapper {
        std::shared_ptr<Scenes::Scene> default_scene;
        std::once_flag default_once;

    public:
        virtual std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() = 0;

        virtual ~SceneWrapper() = default;

        virtual string get_name() {
            return get_default()->get_name();
        }

        std::shared_ptr<Scenes::Scene> get_default() {
            std::call_once(default_once, [this]
            {
                default_scene = create();
                default_scene->update_default_properties();
                default_scene->register_properties();
            });

            return default_scene;
        }
    };
}
