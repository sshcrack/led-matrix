#include "ShadertoyPlugin.h"

#include <shared/matrix/plugin_loader/loader.h>

#include "scenes/ShadertoyScene.h"
#include "providers/Random.h"
#include "providers/Collection.h"
#include "shared/matrix/canvas_consts.h"
#include "spdlog/spdlog.h"

using namespace Scenes;
using namespace ShaderProviders;

extern "C" PLUGIN_EXPORT ShadertoyPlugin *createShadertoy()
{
    return new ShadertoyPlugin();
}

extern "C" PLUGIN_EXPORT void destroyShadertoy(ShadertoyPlugin *c)
{
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>>
ShadertoyPlugin::create_image_providers()
{
    return {};
}

vector<std::unique_ptr<ShaderProviderWrapper, void (*)(ShaderProviderWrapper *)>>
ShadertoyPlugin::create_shader_providers()
{
    auto providers = vector<std::unique_ptr<ShaderProviderWrapper, void (*)(ShaderProviderWrapper *)>>();
    auto deleteWrapper = [](ShaderProviderWrapper *wrapper) {
        delete wrapper;
    };

    providers.push_back({new RandomWrapper(), deleteWrapper});
    providers.push_back({new CollectionWrapper(), deleteWrapper});

    return providers;
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> ShadertoyPlugin::create_scenes()
{
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>>();
    auto deleteScene = [](SceneWrapper *scene)
    {
        delete scene;
    };

    scenes.push_back({new ShadertoySceneWrapper(), deleteScene});

    return scenes;
}

bool ShadertoyPlugin::on_udp_packet(const uint8_t pluginId, const uint8_t *packetData,
                                    const size_t size)
{
    if (pluginId != 0x02)
        return false; // Not destined for this plugin

    int neededPacketSize = Constants::height * Constants::width * 3; // 3 bytes per pixel (RGB)
    static int consecutiveErrors = 0;
    if (size < neededPacketSize)
    {
        consecutiveErrors++;
        if (consecutiveErrors < 10)
            spdlog::error("Received packet too small: {} bytes, expected at least {} bytes", size, neededPacketSize);
        return false; // Invalid packet size
    }

    consecutiveErrors = 0;
    std::unique_lock lock(dataMutex);
    data.assign(packetData, packetData + neededPacketSize);

    return true;
}

std::optional<std::vector<std::string>> ShadertoyPlugin::on_websocket_open()
{
    std::shared_lock lock(Server::currSceneMutex);
    std::string sizeMsg = "size:" + std::to_string(Constants::width) + "x" + std::to_string(Constants::height);
    std::vector<std::string> v = {sizeMsg};
    return v;
}

void ShadertoyPlugin::on_websocket_message(const std::string &message)
{
    if (message == "next_shader")
        Scenes::switchToNextRandomShader = true;
}