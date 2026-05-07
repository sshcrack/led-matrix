#include "ScriptedScenes.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "LatestLuaScene.h"
#include "shared/matrix/plugin_loader/loader.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <sstream>

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

vector<
    std::unique_ptr<Plugins::SceneWrapper, void (*)(Plugins::SceneWrapper *)>>
ScriptedScenes::create_scenes() {
  using WrapperPtr =
      std::unique_ptr<Plugins::SceneWrapper, void (*)(Plugins::SceneWrapper *)>;
  vector<WrapperPtr> scenes;

  if (!fs::exists(lua_scenes_dir)) {
    std::error_code ec;
    fs::create_directories(lua_scenes_dir, ec);
    if (ec) {
      spdlog::warn(
          "[ScriptedScenes] Could not create lua_scenes directory '{}': {}",
          lua_scenes_dir.string(), ec.message());
    } else {
      spdlog::info("[ScriptedScenes] Created lua_scenes directory at '{}'",
                   lua_scenes_dir.string());
    }
    return scenes;
  }

  auto deleter = [](Plugins::SceneWrapper *w) { delete w; };

  for (const auto &entry : fs::directory_iterator(lua_scenes_dir)) {
    if (!entry.is_regular_file())
      continue;
    if (entry.path().extension() != ".lua")
      continue;

    const fs::path &path = entry.path();
    std::string name = path.stem().string();

    // Quick-load the script to read the optional `name` global without
    // keeping a full Lua state alive.
    {
      try {
        sol::state tmp;
        tmp.open_libraries(sol::lib::base);
        auto res = tmp.script_file(
            path.string(), [](lua_State *, sol::protected_function_result pfr) {
              return pfr;
            });
        if (res.valid()) {
          sol::object n = tmp["name"];
          if (n.is<std::string>())
            name = n.as<std::string>();
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

    spdlog::info("[ScriptedScenes] Registering Lua scene '{}' from '{}'", name,
                 path.filename().string());

    try {
      known_files_[name] = fs::last_write_time(path);
    } catch (...) {
      known_files_[name] = std::filesystem::file_time_type::min();
    }

    scenes.emplace_back(new Scenes::LuaSceneWrapper(path, name), deleter);
  }

  scenes.emplace_back(new Scenes::LatestLuaSceneWrapper(), deleter);

  return scenes;
}

std::optional<std::string> ScriptedScenes::after_server_init() {
  stop_watcher_ = false;
  watcher_thread_ = std::thread(&ScriptedScenes::watch_directory, this);
  return std::nullopt;
}

std::optional<std::string> ScriptedScenes::pre_exit() {
  stop_watcher_ = true;
  if (watcher_thread_.joinable()) {
    watcher_thread_.join();
  }
  return std::nullopt;
}

void ScriptedScenes::watch_directory() {
  while (!stop_watcher_) {
    for (int i = 0; i < 10 && !stop_watcher_; i++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (stop_watcher_)
      break;

    if (!fs::exists(lua_scenes_dir))
      continue;

    std::unordered_map<std::string, bool> found_files;
    auto deleter = [](Plugins::SceneWrapper *w) { delete w; };

    for (const auto &entry : fs::directory_iterator(lua_scenes_dir)) {
      if (!entry.is_regular_file())
        continue;
      if (entry.path().extension() != ".lua")
        continue;

      const fs::path &path = entry.path();
      std::string name = path.stem().string();

      // Quick-load the script to read the optional `name` global
      {
        try {
          sol::state tmp;
          tmp.open_libraries(sol::lib::base);
          auto res = tmp.script_file(
              path.string(),
              [](lua_State *, sol::protected_function_result pfr) {
                return pfr;
              });
          if (res.valid()) {
            sol::object n = tmp["name"];
            if (n.is<std::string>())
              name = n.as<std::string>();
          }
        } catch (...) {
        }
      }

      found_files[name] = true;

      if (known_files_.find(name) == known_files_.end()) {
        try {
          known_files_[name] = fs::last_write_time(path);
        } catch (...) {
          known_files_[name] = std::filesystem::file_time_type::min();
        }

        spdlog::info(
            "[ScriptedScenes] Runtime loading new Lua scene '{}' from '{}'",
            name, path.filename().string());

        std::shared_ptr<Plugins::SceneWrapper> wrapper(
            new Scenes::LuaSceneWrapper(path, name), deleter);

        Plugins::PluginManager::instance()->add_scene(std::move(wrapper));
      }
    }

    // Check for deletions
    for (auto it = known_files_.begin(); it != known_files_.end();) {
      if (found_files.find(it->first) == found_files.end()) {
        spdlog::info("[ScriptedScenes] Unloading deleted Lua scene '{}'",
                     it->first);
        Plugins::PluginManager::instance()->remove_scene(it->first);
        it = known_files_.erase(it);
      } else {
        ++it;
      }
    }
  }
}

bool ScriptedScenes::on_udp_packet(const uint8_t pluginId,
                                   const uint8_t *packetData,
                                   const size_t size) {
  if (pluginId != 0x04)
    return false;
  std::lock_guard<std::mutex> lock(dataMutex);
  data.assign(packetData, packetData + size);
  return true;
}

std::vector<uint8_t> ScriptedScenes::get_data() {
  std::lock_guard<std::mutex> lock(dataMutex);
  return data;
}

void ScriptedScenes::set_active_script(std::filesystem::path script_file_path,
                                       std::string sceneName) {
  std::lock_guard<std::mutex> lock(activeScriptPathMutex);
  active_script_path = script_file_path;
  active_scene_name = sceneName;
}

std::optional<std::vector<std::string>> ScriptedScenes::on_websocket_open() {
  std::vector<std::string> messages;

  std::lock_guard<std::mutex> lock(activeScriptPathMutex);
  if (fs::exists(active_script_path)) {
    // Read file content
    std::ifstream file(active_script_path);
    if (file) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      messages.push_back("script:" + active_scene_name + ":" + buffer.str());
    }
  }

  return messages.empty() ? std::nullopt
                          : std::optional<std::vector<std::string>>(messages);
}
