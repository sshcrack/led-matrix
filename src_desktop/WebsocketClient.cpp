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

            const std::string &m = msg->str;
            if (m.starts_with("active:")) {
                activeScene = m.substr(7);
            }

            if (m.starts_with("msg:")) {
                const int pluginNameEnd = m.find(':', 4);

                const std::string pluginName = m.substr(4, pluginNameEnd -4);
                const std::string message = m.substr(m.find(':', pluginNameEnd) +1);

                for (const auto & [_p, plugin] : Plugins::PluginManager::instance()->get_plugins()) {
                    if (plugin->get_plugin_name() != pluginName)
                        continue;

                    plugin->on_websocket_message(message);
                }
            }
        } });

}

WebsocketClient::~WebsocketClient()
{
    ix::initNetSystem();
    senderRunning = false;
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

    for (const auto &plugin: plugins | std::views::values) {
        plugin->udp_init();
    }

    while (senderRunning)
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
            auto packet = pl->compute_next_packet(scene);
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