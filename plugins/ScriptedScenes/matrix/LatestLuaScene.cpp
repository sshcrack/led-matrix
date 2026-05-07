#include "LatestLuaScene.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

static const fs::path lua_scenes_dir = fs::current_path() / "lua_scenes";

namespace Scenes
{
    LatestLuaScene::LatestLuaScene()
    {
        last_check_time_ = clock::now();
    }

    void LatestLuaScene::register_properties()
    {
        // We don't register any properties ourselves.
        // The wrapped LuaScene will initialize its own properties internally using its defaults.
    }

    void LatestLuaScene::initialize(int width, int height)
    {
        Scene::initialize(width, height);
        width_ = width;
        height_ = height;

        // Check for newest scene immediately
        check_for_updates();
    }

    void LatestLuaScene::check_for_updates()
    {
        if (!fs::exists(lua_scenes_dir)) return;

        fs::path newest_path;
        fs::file_time_type newest_time = fs::file_time_type::min();

        try
        {
            for (const auto& entry : fs::directory_iterator(lua_scenes_dir))
            {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != ".lua") continue;

                auto mtime = fs::last_write_time(entry.path());
                if (mtime > newest_time)
                {
                    newest_time = mtime;
                    newest_path = entry.path();
                }
            }
        }
        catch (const fs::filesystem_error& e)
        {
            spdlog::warn("[LatestLuaScene] File system error while scanning dir: {}", e.what());
            return;
        }

        if (!newest_path.empty() && (newest_path != current_script_path_ || newest_time > current_write_time_))
        {
            spdlog::info("[LatestLuaScene] Switching to latest lua scene: {}", newest_path.filename().string());

            auto new_scene = std::make_unique<LuaScene>(newest_path);

            // Setup the new scene
            new_scene->register_properties();

            // Load default properties so they are marked as 'registered'
            nlohmann::json empty_config = nlohmann::json::object();
            for (const auto& prop : new_scene->get_properties())
            {
                prop->load_from_json(empty_config);
            }

            new_scene->initialize(width_, height_);

            current_scene_ = std::move(new_scene);
            current_script_path_ = newest_path;
            current_write_time_ = newest_time;
        }
    }

    bool LatestLuaScene::needs_desktop_app()
    {
        check_for_updates();
        if (current_scene_)
        {
            spdlog::info("[LatestLuaScene] Scene has been loaded");
            return current_scene_->needs_desktop_app();
        }

        spdlog::info("Falling back");
        return false;
    }

    bool LatestLuaScene::render(rgb_matrix::FrameCanvas* canvas)
    {
        auto now = clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check_time_).count() > 500)
        {
            check_for_updates();
            last_check_time_ = now;
        }

        if (current_scene_)
        {
            return current_scene_->render(canvas);
        }

        // If no scene is available, just clear and keep running
        canvas->Clear();
        return true;
    }
} // namespace Scenes
