#include "ShadertoyScene.h"
#include "ShaderMetadata.h"
#include "spdlog/spdlog.h"
#include <shared/matrix/canvas_consts.h>
#include <shared/matrix/utils/LoadingAnimation.h>

#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/config/shader_providers/general.h"
#include <algorithm>
#include <filesystem>
#include <string_view>

bool Scenes::switchToNextRandomShader = true;

using namespace Scenes;
std::string Scenes::ShadertoyScene::lastUrlSent = "";

namespace {
ShadertoyPlugin *get_shadertoy_plugin() {
    auto plugins = Plugins::PluginManager::instance()->get_plugins();
    for (auto &p : plugins) {
        if (auto av = dynamic_cast<ShadertoyPlugin *>(p)) {
            return av;
        }
    }
    return nullptr;
}

void render_plugin_pixels(ShadertoyPlugin *plugin, rgb_matrix::FrameCanvas *canvas) {
    const auto pixels = plugin->get_data();

    if (pixels.empty()) {
        canvas->Fill(0, 0, 255);
        return;
    }

    const int width = Constants::width;
    const int height = Constants::height;
    const uint8_t *data = pixels.data();
    const int num_pixels = pixels.size() / 3;

    for (int idx = 0; idx < num_pixels; ++idx) {
        int x = idx % width;
        int y = height - 1 - (idx / width);
        if (y >= 0) {
            int i = idx * 3;
            canvas->SetPixel(x, y, data[i], data[i + 1], data[i + 2]);
        }
    }
}
}

std::unique_ptr<Scenes::Scene> ShadertoySceneWrapper::create()
{
    return std::make_unique<ShadertoyScene>();
}

ShadertoyScene::ShadertoyScene() : plugin(nullptr)
{
    plugin = get_shadertoy_plugin();

    if (!plugin)
    {
        spdlog::error("ShadertoyScene: Failed to find AudioVisualizer plugin");
    }
}

string ShadertoyScene::get_name() const
{
    return "shadertoy";
}

void ShadertoyScene::register_properties()
{
    add_property(json_providers);
}

void ShadertoyScene::load_properties(const nlohmann::json &j)
{
    Scene::load_properties(j);

    auto is_array = json_providers.get()->get().is_array();
    if (!is_array)
        throw std::runtime_error("Providers of shadertoy scene must be an array");

    auto arr = json_providers->get();
    if (arr.empty())
        throw std::runtime_error("No shader providers given for shadertoy scene");

    for (const auto &provider_json: arr) {
        providers.push_back(ShaderProviders::General::from_json(provider_json));
    }
    
    spdlog::info("ShadertoyScene: Loaded {} shader providers", providers.size());
}

void Scenes::ShadertoyScene::after_render_stop()
{
    switchToNextRandomShader = true;
    failed_provider_count = 0;
    showing_loading_animation = false;
    lastUrlSent.clear();
    if (plugin) {
        const std::string msg = "shadertoy_inactive";
        plugin->send_msg_to_desktop(msg);
        plugin->set_last_sent_message(msg);
    }
}

void Scenes::ShadertoyScene::render_loading_animation(rgb_matrix::FrameCanvas *canvas)
{
    LoadingAnimation::render(canvas, loading_animation_frame++, 0, 255, 255);
}

bool ShadertoyScene::render(rgb_matrix::FrameCanvas *canvas)
{
    if (!plugin)
    {
        spdlog::info("ShadertoyScene: Plugin not found, cannot render");
        hold_current_frame();
        return false;
    }

    if (providers.empty())
    {
        spdlog::error("ShadertoyScene: No providers configured");
        canvas->Fill(255, 0, 0); // Red for error
        return false;
    }

    static std::string url_to_send;

    // Call tick() on current provider for background tasks (like prefetching)
    providers[curr_provider_index]->tick();

    if (switchToNextRandomShader)
    {
        // Get next shader from current provider
        auto result = providers[curr_provider_index]->get_next_shader();
        if (result)
        {
            url_to_send = *result;
            spdlog::info("ShadertoyScene: Got shader URL from provider: {}", url_to_send);
            switchToNextRandomShader = false;
            failed_provider_count = 0;
            showing_loading_animation = false;

            // Rotate to next provider for next time
            curr_provider_index = (curr_provider_index + 1) % providers.size();
        }
        else
        {
            spdlog::warn("ShadertoyScene: Failed to get shader from provider: {}", result.error());
            failed_provider_count++;
            
            // Try next provider
            curr_provider_index = (curr_provider_index + 1) % providers.size();
            
            // If we've tried all providers, flush them and show loading animation
            if (failed_provider_count >= providers.size())
            {
                if (!showing_loading_animation)
                {
                    spdlog::info("ShadertoyScene: All providers failed, flushing all providers and showing loading animation");
                    
                    // Flush all providers
                    for (auto &provider : providers)
                    {
                        provider->flush();
                    }
                    
                    showing_loading_animation = true;
                    loading_animation_frame = 0;
                }
            }
            
            if (showing_loading_animation)
                render_loading_animation(canvas);
            else
                hold_current_frame();
            return true; // Continue rendering without presenting stale canvas contents.
        }
    }

    // If showing loading animation, render it and return
    if (showing_loading_animation)
    {
        render_loading_animation(canvas);
        return true;
    }

    if (!url_to_send.empty() && lastUrlSent != url_to_send)
    {
        spdlog::info("ShadertoyScene: Sending new URL to plugin: {}", url_to_send);
        auto msg = "url:" + url_to_send;
        plugin->send_msg_to_desktop(msg);
        plugin->set_last_sent_message(msg);
        lastUrlSent = url_to_send;
    }

    render_plugin_pixels(plugin, canvas);

    return true;
}

CustomShadertoyScene::CustomShadertoyScene(std::filesystem::path shader_path, std::string scene_prefix)
    : plugin(get_shadertoy_plugin()), shader_path_(std::move(shader_path)), scene_prefix_(std::move(scene_prefix))
{
    scene_name_ = scene_prefix_ + shader_path_.stem().string();

    if (!plugin) {
        spdlog::error("CustomShadertoyScene: Failed to find Shadertoy plugin");
    }
}

string CustomShadertoyScene::get_name() const
{
    return scene_name_;
}

Scenes::SceneDescriptor CustomShadertoyScene::get_descriptor() const
{
    const auto metadata = read_custom_shader_metadata(shader_path_);
    auto descriptor = Scene::get_descriptor();
    descriptor.family = metadata.family;
    descriptor.tags = metadata.tags;
    for (const std::string_view tag : {std::string_view("shader"), std::string_view("shadertoy"), std::string_view("gpu-rendered")}) {
        if (std::find(descriptor.tags.begin(), descriptor.tags.end(), tag) == descriptor.tags.end())
            descriptor.tags.emplace_back(tag);
    }
    descriptor.intensity = metadata.intensity;
    descriptor.motion = metadata.motion;
    descriptor.music_affinity = metadata.music_affinity;
    descriptor.performance_cost = metadata.performance_cost;
    descriptor.automatic_eligible = metadata.automatic_eligible;
    return descriptor;
}

Scenes::SceneCapabilities CustomShadertoyScene::get_capabilities() const
{
    const auto metadata = read_custom_shader_metadata(shader_path_);
    auto caps = Scene::get_capabilities();
    caps.supports_audio = metadata.audio_reactive;
    caps.supports_remote_rendering = false;
    return caps;
}

void CustomShadertoyScene::after_render_stop()
{
    last_shader_sent_.clear();
    if (plugin) {
        const std::string msg = "shadertoy_inactive";
        plugin->send_msg_to_desktop(msg);
        plugin->set_last_sent_message(msg);
    }
}

bool CustomShadertoyScene::render(rgb_matrix::FrameCanvas *canvas)
{
    if (!plugin) {
        spdlog::warn("CustomShadertoyScene: Plugin not found, cannot render");
        hold_current_frame();
        return false;
    }

    const auto shader_file = shader_path_.string();
    if (!std::filesystem::exists(shader_path_)) {
        spdlog::warn("CustomShadertoyScene: Shader file no longer exists: {}", shader_file);
        hold_current_frame();
        return false;
    }

    std::error_code ec;
    auto last_write = std::filesystem::last_write_time(shader_path_, ec);
    std::string current_state = shader_file + ":" + std::to_string(last_write.time_since_epoch().count());

    if (last_shader_sent_ != current_state) {
        std::ifstream file(shader_path_);
        if (!file.is_open()) {
            spdlog::warn("CustomShadertoyScene: Failed to open shader file: {}", shader_file);
        hold_current_frame();
            return false;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        nlohmann::json payload = {
            {"name", shader_path_.stem().string()},
            {"code", buffer.str()}
        };
        auto msg = "custom_shader:" + payload.dump();
        plugin->send_msg_to_desktop(msg);
        plugin->set_last_sent_message(msg);
        last_shader_sent_ = current_state;
    }

    render_plugin_pixels(plugin, canvas);
    return true;
}

CustomShadertoySceneWrapper::CustomShadertoySceneWrapper(std::filesystem::path shader_path, std::string scene_prefix)
    : shader_path_(std::move(shader_path)), scene_prefix_(std::move(scene_prefix))
{
    name_ = scene_prefix_ + shader_path_.stem().string();
}

std::unique_ptr<Scenes::Scene> CustomShadertoySceneWrapper::create()
{
    return std::make_unique<CustomShadertoyScene>(shader_path_, scene_prefix_);
}
