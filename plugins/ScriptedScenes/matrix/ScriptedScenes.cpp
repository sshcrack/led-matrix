#include "ScriptedScenes.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <filesystem>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// The directory scanned for .lua scene scripts.
// Resolved relative to the process working directory, which is the same root
// used for Constants::root_dir ("images/").
// ---------------------------------------------------------------------------
static const fs::path lua_scenes_dir = fs::current_path() / "lua_scenes";

extern "C" PLUGIN_EXPORT ScriptedScenes *createScriptedScenes() {
    return new ScriptedScenes();
}

extern "C" PLUGIN_EXPORT void destroyScriptedScenes(ScriptedScenes *c) {
    delete c;
}

vector<std::unique_ptr<Plugins::SceneWrapper, void (*)(Plugins::SceneWrapper *)>>
ScriptedScenes::create_scenes() {
    using WrapperPtr = std::unique_ptr<Plugins::SceneWrapper, void (*)(Plugins::SceneWrapper *)>;
    vector<WrapperPtr> scenes;

    if (!fs::exists(lua_scenes_dir)) {
        std::error_code ec;
        fs::create_directories(lua_scenes_dir, ec);
        if (ec) {
            spdlog::warn("[ScriptedScenes] Could not create lua_scenes directory '{}': {}",
                         lua_scenes_dir.string(), ec.message());
        } else {
            spdlog::info("[ScriptedScenes] Created lua_scenes directory at '{}'",
                         lua_scenes_dir.string());
        }
        return scenes;
    }

    auto deleter = [](Plugins::SceneWrapper *w) { delete w; };

    for (const auto &entry : fs::directory_iterator(lua_scenes_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".lua") continue;

        const fs::path &path = entry.path();
        std::string name = path.stem().string();

        // Quick-load the script to read the optional `name` global without
        // keeping a full Lua state alive.
        {
            try {
                sol::state tmp;
                tmp.open_libraries(sol::lib::base);
                auto res = tmp.script_file(path.string(),
                    [](lua_State *, sol::protected_function_result pfr) { return pfr; });
                if (res.valid()) {
                    sol::object n = tmp["name"];
                    if (n.is<std::string>()) name = n.as<std::string>();
                } else {
                    sol::error err = res;
                    spdlog::warn("[ScriptedScenes] Skipping '{}' (parse error): {}",
                                 path.filename().string(), err.what());
                    continue;
                }
            } catch (const std::exception &ex) {
                spdlog::warn("[ScriptedScenes] Skipping '{}': {}",
                             path.filename().string(), ex.what());
                continue;
            }
        }

        spdlog::info("[ScriptedScenes] Registering Lua scene '{}' from '{}'",
                     name, path.filename().string());

        scenes.emplace_back(new Scenes::LuaSceneWrapper(path, name), deleter);
    }

    return scenes;
}
