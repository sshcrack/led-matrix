#include "ShadertoyScene.h"
#include "spdlog/spdlog.h"
#include <shared/matrix/canvas_consts.h>

#include "shared/matrix/plugin_loader/loader.h"

using namespace Scenes;
std::string Scenes::ShadertoyScene::lastUrlSent = "";

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> ShadertoySceneWrapper::create() {
    return {
        new ShadertoyScene(), [](Scenes::Scene *scene) {
            delete scene;
        }
    };
}

ShadertoyScene::ShadertoyScene() : plugin(nullptr) {
    // Find the AudioVisualizer plugin
    auto plugins = Plugins::PluginManager::instance()->get_plugins();
    for (auto &p: plugins) {
        if (auto av = dynamic_cast<ShadertoyPlugin *>(p)) {
            plugin = av;
            break;
        }
    }

    if (!plugin) {
        spdlog::error("ShadertoyScene: Failed to find AudioVisualizer plugin");
    }
}

string ShadertoyScene::get_name() const {
    return "shadertoy";
}

void ShadertoyScene::register_properties() {
    add_property(toy_url);
}


bool ShadertoyScene::render(RGBMatrixBase *matrix) {
    if (!plugin) {
        spdlog::info("ShadertoyScene: Plugin not found, cannot render");
        return false;
    }

    if (lastUrlSent != toy_url->get()) {
        spdlog::info("ShadertoyScene: Sending new URL to plugin: {}", toy_url->get());
        plugin->send_msg_to_desktop("url:" + toy_url->get());
        lastUrlSent = toy_url->get();
    }

    const auto pixels = plugin->get_data();
    if(pixels.empty()) {
        offscreen_canvas->Fill(0, 0, 255); // Clear the canvas if no data
    }

    for (int i = 0; i < pixels.size(); i += 3) {
        const int x = (i / 3) % Constants::width;
        const int y = (i / 3) / Constants::width;

        offscreen_canvas->SetPixel(x, y, pixels[i], pixels[i +1], pixels[i +2]);
    }

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    return true;
}
