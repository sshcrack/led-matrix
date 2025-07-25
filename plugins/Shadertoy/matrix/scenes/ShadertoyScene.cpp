#include "ShadertoyScene.h"
#include "../scraper/Scraper.h"
#include "spdlog/spdlog.h"
#include <shared/matrix/canvas_consts.h>
#include <future>

#include "shared/matrix/plugin_loader/loader.h"
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
    add_property(toy_url);
    add_property(random_shader);
    add_property(min_page);
    add_property(max_page);
}

void ShadertoyScene::prefetch_random_shader()
{
    if (random_shader->get())
    {
        int minPage = min_page->get();
        int maxPage = max_page->get();
        Scraper::instance().prefetchShadersAsync(minPage, maxPage);
    }
}

void Scenes::ShadertoyScene::after_render_stop(RGBMatrixBase *matrix)
{
    switchToNextRandomShader = true;
    if (random_shader->get())
    {
        prefetch_random_shader();
    }
}

bool ShadertoyScene::render(RGBMatrixBase *matrix)
{
    if (!plugin)
    {
        spdlog::info("ShadertoyScene: Plugin not found, cannot render");
        return false;
    }
    static bool waiting_for_shader = false;
    static int loading_anim_frame = 0;
    static std::string url_to_send;

    if ((random_shader->get() && switchToNextRandomShader))
    {
        if (!Scraper::instance().hasShadersLeft())
        {
            // Start async fetch if not already running
            Scraper::instance().prefetchShadersAsync(min_page->get(), max_page->get());
            // Show loading animation: a line of 5 pixels going around the frame
            int w = Constants::width, h = Constants::height;
            int perimeter = 2 * (w + h) - 4;
            int pos = (loading_anim_frame / 500) % perimeter; // slow down by factor 500
            offscreen_canvas->Fill(0, 0, 0);
            
            // Draw a line of 15 pixels
            for (int i = 0; i < 15; i++)
            {
                int current_pos = (pos + i) % perimeter;
                int x = 0, y = 0;
                
                if (current_pos < w)
                {
                    x = current_pos;
                    y = 0;
                }
                else if (current_pos < w + h - 1)
                {
                    x = w - 1;
                    y = current_pos - w + 1;
                }
                else if (current_pos < 2 * w + h - 2)
                {
                    x = w - 1 - (current_pos - (w + h - 1));
                    y = h - 1;
                }
                else
                {
                    x = 0;
                    y = h - 1 - (current_pos - (2 * w + h - 2));
                }
                offscreen_canvas->SetPixel(x, y, 255, 255, 255);
            }
            loading_anim_frame++;
            waiting_for_shader = true;

            offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
            return true;
        }
        else
        {
            if (waiting_for_shader)
            {
                spdlog::info("ShadertoyScene: Shader(s) now available after loading.");
                waiting_for_shader = false;
            }
            auto result = Scraper::instance().scrapeNextShader(min_page->get(), max_page->get());
            if (result)
            {
                url_to_send = *result;
                spdlog::info("ShadertoyScene: Got random shader URL: {}", url_to_send);
            }
            else
            {
                spdlog::warn("ShadertoyScene: Could not get random shader: {}", result.error());
                url_to_send = toy_url->get(); // fallback to current
            }

            switchToNextRandomShader = false;
        }
    }
    if (!random_shader->get())
        url_to_send = toy_url->get();

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

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    return true;
}
