#pragma once

#include "../ShadertoyPlugin.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/config/shader_providers/general.h"
#include "shared/matrix/wrappers.h"
#include <filesystem>

namespace Scenes {
    extern bool switchToNextRandomShader;

    class ShadertoyScene : public Scene {
        // Make lastUrlSent a static member variable, declared here
        static std::string lastUrlSent;
        ShadertoyPlugin *plugin;

        std::vector<std::shared_ptr<ShaderProviders::General> > providers;
        uint curr_provider_index = 0;
        uint failed_provider_count = 0;
        bool showing_loading_animation = false;
        uint loading_animation_frame = 0;

        void render_loading_animation(rgb_matrix::FrameCanvas *canvas);

    public:
        ShadertoyScene();

        ~ShadertoyScene() override = default;

        bool render(rgb_matrix::FrameCanvas *canvas) override;

        string get_name() const override;
        std::string getCategory() const override { return "Shaders"; }

        void register_properties() override;

        void load_properties(const nlohmann::json &j) override;

        tmillis_t get_default_duration() override { return 20000; }

        int get_default_weight() override { return 5; }

        void after_render_stop() override;

        // Properties for the scene
        PropertyPointer<nlohmann::json> json_providers = MAKE_PROPERTY(
            "shader_providers", nlohmann::json,
            nlohmann::json::parse(
                R"([{"type":"random","arguments":{"min_page":0,"max_page":100}}])"));

        [[nodiscard]] bool needs_desktop_app() override {
            return true; // This scene requires audio data from the desktop application
        }
    };

    class ShadertoySceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };

    class CustomShadertoyScene : public Scene {
        ShadertoyPlugin *plugin;
        std::filesystem::path shader_path_;
        std::string scene_name_;
        std::string last_shader_sent_;

    public:
        explicit CustomShadertoyScene(std::filesystem::path shader_path);
        ~CustomShadertoyScene() override = default;

        bool render(rgb_matrix::FrameCanvas *canvas) override;
        string get_name() const override;
        std::string getCategory() const override { return "Custom Shaders"; }
        void register_properties() override {}
        tmillis_t get_default_duration() override { return 20000; }
        int get_default_weight() override { return 5; }
        [[nodiscard]] bool needs_desktop_app() override { return true; }
    };

    class CustomShadertoySceneWrapper : public Plugins::SceneWrapper {
        std::filesystem::path shader_path_;
        std::string name_;

    public:
        explicit CustomShadertoySceneWrapper(std::filesystem::path shader_path);
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
        std::string get_name() override { return name_; }
    };
} // namespace Scenes
