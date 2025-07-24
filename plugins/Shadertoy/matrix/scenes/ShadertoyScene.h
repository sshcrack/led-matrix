#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "../ShadertoyPlugin.h"

namespace Scenes {
    class ShadertoyScene : public Scene {
        // Make lastUrlSent a static member variable, declared here
        static std::string lastUrlSent;
        ShadertoyPlugin* plugin;

        std::string currRandomUrl;

    public:
        ShadertoyScene();
        ~ShadertoyScene() override = default;

        bool render(rgb_matrix::RGBMatrixBase *matrix) override;
        string get_name() const override;
        void register_properties() override;
        void prefetch_random_shader();

        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 5;
        }

        void after_render_stop(RGBMatrixBase *matrix) override;

        // Properties for the scene
        PropertyPointer<std::string> toy_url = MAKE_PROPERTY("url", std::string, "https://www.shadertoy.com/view/33cGDj");
        PropertyPointer<bool> random_shader = MAKE_PROPERTY("random_shader", bool, true);
        PropertyPointer<int> min_page = MAKE_PROPERTY_MINMAX("min_page", int, 0, 0, 8819);
        PropertyPointer<int> max_page = MAKE_PROPERTY_MINMAX("max_page", int, 100, 0, 8819);
    };

    class ShadertoySceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
