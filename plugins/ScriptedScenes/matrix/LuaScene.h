#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"

#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <utility>

// Forward-declare sol2 types to keep the header clean (sol.hpp is large).
namespace sol
{
    class state;
}

namespace Scenes
{
    /// One Lua scene instance wrapping a single .lua script file.
    class LuaScene : public Scene
    {
    public:
        explicit LuaScene(std::filesystem::path script_path);
        ~LuaScene() override;

        bool render(rgb_matrix::FrameCanvas* canvas) override;
        std::string get_name() const override { return scene_name_; }
        void register_properties() override;
        void initialize(int width, int height) override;

        tmillis_t get_default_duration() override { return 10000; }
        int get_default_weight() override { return 5; }

        [[nodiscard]] bool needs_desktop_app() override;

    private:
        std::filesystem::path script_path_;
        std::string scene_name_;

        std::unique_ptr<sol::state> lua_;
        FrameTimer frame_timer_;

        std::filesystem::file_time_type last_write_time_{};
        bool lua_loaded_ = false;
        bool is_first_load_ = true;
        bool offload_render_ = false;
        bool external_render_only_ = false;

        /// Canvas pointer valid only inside render(); used by set_pixel / clear callbacks.
        rgb_matrix::FrameCanvas* current_canvas_ = nullptr;

        /// Tracks dynamic property name → type string so get_property can cast correctly.
        struct LuaPropEntry
        {
            std::string name;
            std::string type;
            std::shared_ptr<Plugins::PropertyBase> prop;
        };

        std::vector<LuaPropEntry> lua_props_;

        void setup_lua_state();
        bool load_and_exec_script();
        bool had_changes_to_script() const;
        void call_setup();
        void call_initialize_fn();

        /// Read a property value and return it as a Lua-compatible number or string.
        double get_prop_as_number(const LuaPropEntry& entry) const;
        std::string get_prop_as_string(const LuaPropEntry& entry) const;
    };

    /// SceneWrapper that instantiates LuaScene for a given script path.
    class LuaSceneWrapper : public Plugins::SceneWrapper
    {
    public:
        LuaSceneWrapper(std::filesystem::path path, std::string name)
            : script_path_(std::move(path)), cached_name_(std::move(name))
        {
        }

        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene*)> create() override
        {
            return {new LuaScene(script_path_), [](Scenes::Scene* s) { delete s; }};
        }

        /// Return the pre-scanned name so we don't need to spin up a Lua state
        /// just to populate the scene list.
        std::string get_name() override { return cached_name_; }

    private:
        std::filesystem::path script_path_;
        std::string cached_name_;
    };
} // namespace Scenes
