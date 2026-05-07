#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "LuaScene.h"
#include <filesystem>
#include <memory>
#include <chrono>

namespace Scenes {

    class LatestLuaScene : public Scene {
    public:
        LatestLuaScene();
        ~LatestLuaScene() override = default;

        bool render(rgb_matrix::FrameCanvas *canvas) override;
        std::string get_name() const override { return "Latest Lua Scene"; }
        void register_properties() override;
        void initialize(int width, int height) override;

        tmillis_t get_default_duration() override { return 10000; }
        int get_default_weight() override { return 5; }

        bool needs_desktop_app() override;

    private:
        std::unique_ptr<LuaScene> current_scene_;
        std::filesystem::path current_script_path_;
        std::filesystem::file_time_type current_write_time_{};

        // Cache matrix dimensions
        int width_ = 128;
        int height_ = 128;

        // Rate limit file system checks
        using clock = std::chrono::steady_clock;
        clock::time_point last_check_time_;

        void check_for_updates();
    };

    class LatestLuaSceneWrapper : public Plugins::SceneWrapper {
    public:
        LatestLuaSceneWrapper() = default;

        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override {
            return {new LatestLuaScene(), [](Scenes::Scene *s) { delete s; }};
        }

        std::string get_name() override { return "Latest Lua Scene"; }
    };

} // namespace Scenes
