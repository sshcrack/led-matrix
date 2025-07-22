#include "WebsocketClient.h"
#include <spdlog/spdlog.h>
#include <shared/desktop/plugin_loader/loader.h>
#include <ixwebsocket/IXNetSystem.h>

WebsocketClient::WebsocketClient() : udpSender()
{
    ix::initNetSystem();
    webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg)
                                   {
        if (msg->type == ix::WebSocketMessageType::Message)
        {
            std::unique_lock<std::mutex> lock(activeSceneMutex);
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
    auto configManager = Config::ConfigManager::instance();

    auto generalConfig = configManager->getGeneralConfig();
    std::string hostname = generalConfig.getHostname();
    uint16_t port = generalConfig.getPort();
    auto lastUpdated = clock::now();

    while (serverRunning)
    {
        auto frame_start = clock::now();
        std::string scene = getActiveScene();

        if (lastUpdated + std::chrono::seconds(1) < frame_start)
        {
            lastUpdated = frame_start;

            generalConfig = configManager->getGeneralConfig();
            hostname = generalConfig.getHostname();
            port = generalConfig.getPort();
        }

        for (auto &pl: plugins | std::views::values)
        {
            auto packet = pl->onNextPacket(scene);
            if (!packet.has_value())
                continue;

            auto res = this->udpSender.sendPacket(std::move(packet.value()), hostname, port);
            static int consecutiveError = 0;
            if (!res.has_value())
            {
                std::unique_lock<std::mutex> lock(lastErrorMutex);
                lastError = res.error();
                consecutiveError++;

                if (consecutiveError < 3)
                spdlog::error("Failed to send packet: {}", lastError);
            }
            else
            {
                std::unique_lock<std::mutex> lock(lastErrorMutex);
                lastError.clear(); // Clear error on successful send

                consecutiveError = 0;
            }
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