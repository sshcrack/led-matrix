#include "WebsocketClient.h"
#include <spdlog/spdlog.h>
#include <shared/desktop/plugin_loader/loader.h>
#include <ixwebsocket/IXNetSystem.h>

WebsocketClient::WebsocketClient() : udpSender()
{
    ix::initNetSystem();
    webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg)
                                   {
        spdlog::info("WebSocket message received: {}", (int) msg->type);
        if (msg->type == ix::WebSocketMessageType::Message)
        {
            std::unique_lock<std::mutex> lock(activeSceneMutex);
            spdlog::info("WebSocket message: {}", msg->str);
            activeScene = msg->str;
        } });

    senderThread = std::thread(&WebsocketClient::threadLoop, this);
}

WebsocketClient::~WebsocketClient()
{
    ix::initNetSystem();
    serverRunning = false;
    if (senderThread.joinable())
    {
        senderThread.join();
    }
}

constexpr double TARGET_FPS = 60.0;
constexpr std::chrono::duration<double, std::milli> FRAME_DURATION_MS(1000.0 / TARGET_FPS);

void WebsocketClient::threadLoop()
{
    using clock = std::chrono::high_resolution_clock;
    auto plugins = Plugins::PluginManager::instance()->get_plugins();

    while (serverRunning)
    {
        auto frame_start = clock::now();
        std::string scene = getActiveScene();
        for (auto &[name, pl] : plugins)
        {
            auto packet = pl->onNextPacket(scene);
        }

        auto frame_end = clock::now();
        auto elapsed = frame_end - frame_start;

        // If frame took less than target, sleep the remaining time
        if (elapsed < FRAME_DURATION_MS)
        {
            std::this_thread::sleep_for(FRAME_DURATION_MS - elapsed);
        }
    }
}