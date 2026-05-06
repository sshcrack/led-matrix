#include "LuaScene.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>
#include <cstdint>

// ---------------------------------------------------------------------------
// Helper: extract the Lua `name` global from a file without keeping state.
// Returns an empty string if the file cannot be loaded or the global is absent.
// ---------------------------------------------------------------------------
static std::string quick_read_name(const std::filesystem::path &path) {
    try {
        sol::state tmp;
        tmp.open_libraries(sol::lib::base);
        auto result = tmp.script_file(path.string(), [](lua_State *, sol::protected_function_result pfr) {
            return pfr;
        });
        if (!result.valid()) return "";
        sol::object name_obj = tmp["name"];
        if (name_obj.is<std::string>()) return name_obj.as<std::string>();
    } catch (...) {}
    return "";
}

namespace Scenes {

LuaScene::LuaScene(std::filesystem::path script_path)
    : script_path_(std::move(script_path)) {
    scene_name_ = script_path_.stem().string();
}

LuaScene::~LuaScene() = default;

// ---------------------------------------------------------------------------
// Initialise the Lua state and bind all C++ → Lua API functions.
// This is called on first load and on every hot-reload.
// ---------------------------------------------------------------------------
void LuaScene::setup_lua_state() {
    lua_ = std::make_unique<sol::state>();
    lua_->open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table
    );

    // ---- set_pixel(x, y, r, g, b) ----------------------------------------
    lua_->set_function("set_pixel", [this](int x, int y, int r, int g, int b) {
        if (!current_canvas_) return;
        if (x < 0 || x >= matrix_width || y < 0 || y >= matrix_height) return;
        current_canvas_->SetPixel(x, y,
            static_cast<uint8_t>(std::clamp(r, 0, 255)),
            static_cast<uint8_t>(std::clamp(g, 0, 255)),
            static_cast<uint8_t>(std::clamp(b, 0, 255)));
    });

    // ---- clear() ----------------------------------------------------------
    lua_->set_function("clear", [this]() {
        if (current_canvas_) current_canvas_->Clear();
    });

    // ---- log(msg) ---------------------------------------------------------
    lua_->set_function("log", [this](const std::string &msg) {
        spdlog::info("[LuaScene:{}] {}", scene_name_, msg);
    });

    // ---- define_property(name, type, default [, min, max]) ----------------
    lua_->set_function("define_property",
        [this](const std::string &name, const std::string &type,
               sol::object default_val, sol::variadic_args va)
    {
        if (!is_first_load_) {
            // Hot-reload: property shapes are fixed; just ignore re-definitions.
            return;
        }

        // Check for duplicates (can happen if the script calls setup() twice).
        for (const auto &e : lua_props_) {
            if (e.name == name) return;
        }

        try {
            LuaPropEntry entry;
            entry.name = name;
            entry.type = type;

            if (type == "float") {
                float def = default_val.is<double>() ? static_cast<float>(default_val.as<double>()) : 0.f;
                std::optional<float> mn, mx;
                if (va.size() >= 1) mn = static_cast<float>(va[0].as<double>());
                if (va.size() >= 2) mx = static_cast<float>(va[1].as<double>());
                auto prop = std::make_shared<Plugins::Property<float>>(name, def, false, mn, mx);
                add_property(prop);
                entry.prop = prop;

            } else if (type == "int") {
                int def = default_val.is<double>() ? static_cast<int>(default_val.as<double>()) : 0;
                std::optional<int> mn, mx;
                if (va.size() >= 1) mn = static_cast<int>(va[0].as<double>());
                if (va.size() >= 2) mx = static_cast<int>(va[1].as<double>());
                auto prop = std::make_shared<Plugins::Property<int>>(name, def, false, mn, mx);
                add_property(prop);
                entry.prop = prop;

            } else if (type == "bool") {
                bool def = default_val.is<bool>() ? default_val.as<bool>() : false;
                auto prop = std::make_shared<Plugins::Property<bool>>(name, def, false);
                add_property(prop);
                entry.prop = prop;

            } else if (type == "string") {
                std::string def = default_val.is<std::string>() ? default_val.as<std::string>() : "";
                auto prop = std::make_shared<Plugins::Property<std::string>>(name, def, false);
                add_property(prop);
                entry.prop = prop;

            } else if (type == "color") {
                // Lua passes colors as 0xRRGGBB integers.
                uint32_t raw = default_val.is<double>()
                    ? static_cast<uint32_t>(default_val.as<double>()) : 0u;
                rgb_matrix::Color def{
                    static_cast<uint8_t>((raw >> 16) & 0xFF),
                    static_cast<uint8_t>((raw >> 8)  & 0xFF),
                    static_cast<uint8_t>( raw         & 0xFF)
                };
                auto prop = std::make_shared<Plugins::Property<rgb_matrix::Color>>(name, def, false);
                add_property(prop);
                entry.prop = prop;

            } else {
                spdlog::warn("[LuaScene:{}] Unknown property type '{}' for '{}', skipping",
                             scene_name_, type, name);
                return;
            }

            lua_props_.push_back(std::move(entry));

        } catch (const std::exception &ex) {
            spdlog::error("[LuaScene:{}] define_property('{}') failed: {}",
                          scene_name_, name, ex.what());
        }
    });

    // ---- get_property(name) → number | string | bool | nil ---------------
    lua_->set_function("get_property", [this](const std::string &name) -> sol::object {
        for (const auto &e : lua_props_) {
            if (e.name != name) continue;

            if (e.type == "float") {
                auto p = std::static_pointer_cast<Plugins::Property<float>>(e.prop);
                return sol::make_object(*lua_, static_cast<double>(p->get()));
            }
            if (e.type == "int") {
                auto p = std::static_pointer_cast<Plugins::Property<int>>(e.prop);
                return sol::make_object(*lua_, static_cast<double>(p->get()));
            }
            if (e.type == "bool") {
                auto p = std::static_pointer_cast<Plugins::Property<bool>>(e.prop);
                return sol::make_object(*lua_, p->get());
            }
            if (e.type == "string") {
                auto p = std::static_pointer_cast<Plugins::Property<std::string>>(e.prop);
                return sol::make_object(*lua_, p->get());
            }
            if (e.type == "color") {
                auto p = std::static_pointer_cast<Plugins::Property<rgb_matrix::Color>>(e.prop);
                auto c = p->get();
                uint32_t val = (static_cast<uint32_t>(c.r) << 16)
                             | (static_cast<uint32_t>(c.g) << 8)
                             |  static_cast<uint32_t>(c.b);
                return sol::make_object(*lua_, static_cast<double>(val));
            }
        }
        return sol::nil;
    });

    // ---- static width / height globals (updated in initialize) -----------
    (*lua_)["width"]  = matrix_width;
    (*lua_)["height"] = matrix_height;
    (*lua_)["time"]   = 0.0;
    (*lua_)["dt"]     = 0.0;
}

// ---------------------------------------------------------------------------
bool LuaScene::load_and_exec_script() {
    auto result = lua_->script_file(
        script_path_.string(),
        [](lua_State *, sol::protected_function_result pfr) { return pfr; }
    );

    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("[LuaScene:{}] Script load error: {}", scene_name_, err.what());
        lua_loaded_ = false;
        return false;
    }

    // Override scene name from Lua global if present.
    sol::object name_obj = (*lua_)["name"];
    if (name_obj.is<std::string>()) {
        scene_name_ = name_obj.as<std::string>();
    }

    try {
        last_write_time_ = std::filesystem::last_write_time(script_path_);
    } catch (...) {}

    lua_loaded_ = true;
    return true;
}

// ---------------------------------------------------------------------------
void LuaScene::call_setup() {
    sol::protected_function fn = (*lua_)["setup"];
    if (!fn.valid()) return;

    auto result = fn();
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("[LuaScene:{}] setup() error: {}", scene_name_, err.what());
    }
}

void LuaScene::call_initialize_fn() {
    sol::protected_function fn = (*lua_)["initialize"];
    if (!fn.valid()) return;

    auto result = fn();
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("[LuaScene:{}] initialize() error: {}", scene_name_, err.what());
    }
}

// ---------------------------------------------------------------------------
void LuaScene::register_properties() {
    setup_lua_state();
    if (load_and_exec_script()) {
        call_setup();
    }
    is_first_load_ = false;
}

// ---------------------------------------------------------------------------
void LuaScene::initialize(int width, int height) {
    Scene::initialize(width, height);

    // Update width/height globals in the Lua state.
    if (lua_) {
        (*lua_)["width"]  = matrix_width;
        (*lua_)["height"] = matrix_height;
    }

    call_initialize_fn();
}

// ---------------------------------------------------------------------------
bool LuaScene::render(rgb_matrix::FrameCanvas *canvas) {
    if (!lua_) return true;

    // --- hot-reload: check if the script file has changed -----------------
    try {
        auto mtime = std::filesystem::last_write_time(script_path_);
        if (mtime != last_write_time_) {
            spdlog::info("[LuaScene] Hot-reloading '{}'", script_path_.filename().string());
            setup_lua_state();
            if (load_and_exec_script()) {
                call_setup();
                // Re-run initialize with existing dimensions.
                (*lua_)["width"]  = matrix_width;
                (*lua_)["height"] = matrix_height;
                call_initialize_fn();
            }
            // Reset timer so `time` restarts from 0 after a reload.
            frame_timer_ = FrameTimer{};
        }
    } catch (const std::filesystem::filesystem_error &) {
        // File briefly unavailable (e.g. being written) – skip this frame.
        return true;
    }

    if (!lua_loaded_) return true;

    auto frame = frame_timer_.tick();
    (*lua_)["time"] = frame.t;
    (*lua_)["dt"]   = frame.dt;

    sol::protected_function render_fn = (*lua_)["render"];
    if (!render_fn.valid()) return true;

    current_canvas_ = canvas;
    auto result = render_fn();
    current_canvas_ = nullptr;

    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("[LuaScene:{}] render() error: {}", scene_name_, err.what());
        return true; // keep running despite errors
    }

    // render() should return true to keep running, false to stop.
    sol::object ret_val = result;
    if (ret_val.is<bool>()) return ret_val.as<bool>();
    return true;
}

} // namespace Scenes
