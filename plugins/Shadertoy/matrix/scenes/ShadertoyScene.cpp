#include "ShadertoyScene.h"
#include "spdlog/spdlog.h"
#include <shared/matrix/canvas_consts.h>

#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/config/shader_providers/general.h"

bool Scenes::switchToNextRandomShader = true;

using namespace Scenes;
std::string Scenes::ShadertoyScene::lastUrlSent = "";

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> ShadertoySceneWrapper::create()
{
    return {
        new ShadertoyScene(), [](Scenes::Scene *scene)
        {
            delete scene;
        }};
}

ShadertoyScene::ShadertoyScene() : plugin(nullptr)
{
    // Find the AudioVisualizer plugin
    auto plugins = Plugins::PluginManager::instance()->get_plugins();
    for (auto &p : plugins)
    {
        if (auto av = dynamic_cast<ShadertoyPlugin *>(p))
        {
            plugin = av;
            break;
        }
    }

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

void Scenes::ShadertoyScene::after_render_stop(RGBMatrixBase *matrix)
{
    switchToNextRandomShader = true;
    failed_provider_count = 0;
    showing_loading_animation = false;
}

void Scenes::ShadertoyScene::render_loading_animation()
{
    const int width = Constants::width;
    const int height = Constants::height;
    
    // Clear canvas
    offscreen_canvas->Fill(0, 0, 0);
    
    // Simple rotating dots animation
    const int num_dots = 8;
    const int radius = std::min(width, height) / 3;
    const int center_x = width / 2;
    const int center_y = height / 2;
    
    for (int i = 0; i < num_dots; ++i)
    {
        float angle = (2.0f * M_PI * i / num_dots) + (loading_animation_frame * 0.05f);
        int x = center_x + static_cast<int>(radius * cos(angle));
        int y = center_y + static_cast<int>(radius * sin(angle));
        
        // Fade dots based on position in rotation
        int brightness = 255 - (i * 255 / num_dots);
        
        // Draw a small circle for each dot
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < width && py >= 0 && py < height)
                {
                    offscreen_canvas->SetPixel(px, py, 0, brightness, brightness);
                }
            }
        }
    }
    
    loading_animation_frame++;
}

bool ShadertoyScene::render(RGBMatrixBase *matrix)
{
    if (!plugin)
    {
        spdlog::info("ShadertoyScene: Plugin not found, cannot render");
        return false;
    }

    if (providers.empty())
    {
        spdlog::error("ShadertoyScene: No providers configured");
        offscreen_canvas->Fill(255, 0, 0); // Red for error
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
            
            return true; // Continue rendering
        }
    }

    // If showing loading animation, render it and return
    if (showing_loading_animation)
    {
        render_loading_animation();
        return true;
    }

    if (!url_to_send.empty() && lastUrlSent != url_to_send)
    {
        spdlog::info("ShadertoyScene: Sending new URL to plugin: {}", url_to_send);
        plugin->send_msg_to_desktop("url:" + url_to_send);
        lastUrlSent = url_to_send;
    }

    const auto pixels = plugin->get_data();

    if (pixels.empty())
    {
        offscreen_canvas->Fill(0, 0, 255); // Clear the canvas if no data
    }
    // Use pointers for faster access and avoid repeated division/modulo
    const int width = Constants::width;
    const int height = Constants::height;
    const uint8_t *data = pixels.data();
    const int num_pixels = pixels.size() / 3;

    for (int idx = 0; idx < num_pixels; ++idx)
    {
        int x = idx % width;
        int y = height - 1 - (idx / width);
        if (y >= 0)
        {
            int i = idx * 3;
            offscreen_canvas->SetPixel(x, y, data[i], data[i + 1], data[i + 2]);
        }
    }

    return true;
}
