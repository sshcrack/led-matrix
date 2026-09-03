#include "shared/desktop/WebsocketClient.h"
#include <spdlog/spdlog.h>
#include "shared/desktop/plugin_loader/loader.h"
#include <ixwebsocket/IXNetSystem.h>

WebsocketClient *websocketClientInstance = nullptr;

void WebsocketClient::setInstance(WebsocketClient *instance)
{
    websocketClientInstance = instance;
}

WebsocketClient *WebsocketClient::instance()
{
    return websocketClientInstance;
}

std::shared_ptr<WebsocketClient> WebsocketClient::create()
{
    auto instance = std::shared_ptr<WebsocketClient>(new WebsocketClient());
    instance->setup_callback();
    return instance;
}

WebsocketClient::WebsocketClient() : udpSender()
{
    if (net_refs().fetch_add(1, std::memory_order_relaxed) == 0) {
        ix::initNetSystem();
    }
    pluginMessageThread_ = std::thread(&WebsocketClient::pluginMessageLoop, this);
}

void WebsocketClient::setup_callback()
{
    std::weak_ptr<WebsocketClient> weak_this = shared_from_this();
    webSocket.setOnMessageCallback([weak_this](const ix::WebSocketMessagePtr &msg)
                                   {
        auto shared_this = weak_this.lock();
        if (!shared_this) return;

        if (msg->type == ix::WebSocketMessageType::Open)
        {
            shared_this->streamState_.on_websocket_open();
            {
                std::unique_lock<std::mutex> lock(shared_this->lastErrorMutex);
                shared_this->lastError.clear();
            }
            spdlog::info("WebSocket opened");
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Close)
        {
            shared_this->streamState_.on_websocket_closed();
            spdlog::warn("WebSocket closed (code {}, remote={}): {}",
                         msg->closeInfo.code, msg->closeInfo.remote,
                         msg->closeInfo.reason.empty() ? "no reason" : msg->closeInfo.reason);
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Error)
        {
            shared_this->streamState_.on_websocket_closed();
            {
                std::unique_lock<std::mutex> lock(shared_this->lastErrorMutex);
                shared_this->lastError = msg->errorInfo.reason.empty()
                    ? "WebSocket transport error"
                    : msg->errorInfo.reason;
            }
            spdlog::warn("WebSocket error (retry {}, wait {}s, HTTP {}): {}",
                         msg->errorInfo.retries, msg->errorInfo.wait_time,
                         msg->errorInfo.http_status,
                         msg->errorInfo.reason.empty() ? "no reason" : msg->errorInfo.reason);
            return;
        }

        if (msg->type == ix::WebSocketMessageType::Message)
        {
            const std::string &m = msg->str;
            if (shared_this->streamState_.on_control_message(m))
                return;

            if (m.starts_with("active:")) {
                std::unique_lock<std::mutex> lock(shared_this->activeSceneMutex);
                shared_this->activeScene = m.substr(7);
            }

            if (m.starts_with("msg:")) {
                const auto pluginNameEnd = m.find(':', 4);
                if (pluginNameEnd == std::string::npos)
                    return;
                const std::string pluginName = m.substr(4, pluginNameEnd - 4);
                const std::string message = m.substr(pluginNameEnd + 1);
                shared_this->enqueuePluginMessage(pluginName, message);
            }
        } });
}

WebsocketClient::~WebsocketClient()
{
    transportStarted_.store(false);
    webSocket.disableAutomaticReconnection();
    webSocket.stop();
    senderRunning.store(false);
    if (senderThread.joinable())
    {
        senderThread.join();
    }
    {
        std::lock_guard<std::mutex> lock(pluginMessageMutex_);
        pluginMessageRunning_.store(false);
        pluginMessages_.clear();
    }
    pluginMessageCv_.notify_all();
    if (pluginMessageThread_.joinable())
        pluginMessageThread_.join();
    if (net_refs().fetch_sub(1, std::memory_order_relaxed) == 1) {
        ix::uninitNetSystem();
    }
}

void WebsocketClient::enqueuePluginMessage(std::string plugin_name, std::string message)
{
    {
        std::lock_guard<std::mutex> lock(pluginMessageMutex_);
        pluginMessages_.emplace_back(std::move(plugin_name), std::move(message));
    }
    pluginMessageCv_.notify_one();
}

void WebsocketClient::pluginMessageLoop()
{
    while (true) {
        std::pair<std::string, std::string> item;
        {
            std::unique_lock<std::mutex> lock(pluginMessageMutex_);
            pluginMessageCv_.wait(lock, [this] {
                return !pluginMessageRunning_.load() || !pluginMessages_.empty();
            });
            if (!pluginMessageRunning_.load() && pluginMessages_.empty())
                return;
            item = std::move(pluginMessages_.front());
            pluginMessages_.pop_front();
        }

        try {
            for (const auto& [_name, plugin] : Plugins::PluginManager::instance()->get_plugins()) {
                if (plugin->get_plugin_name() != item.first)
                    continue;
                plugin->on_websocket_message(item.second);
                break;
            }
        } catch (const std::exception& e) {
            spdlog::error("Desktop plugin '{}' WebSocket message failed: {}", item.first, e.what());
        } catch (...) {
            spdlog::error("Desktop plugin '{}' WebSocket message failed with unknown exception", item.first);
        }
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
    int udpFpsLimit = generalConfig.getUdpFpsLimit();
    auto lastUpdated = clock::now();

    std::unordered_map<std::string, clock::time_point> lastLargePayloadSend;

    for (const auto &plugin : plugins | std::views::values)
    {
        plugin->udp_init();
        lastLargePayloadSend[plugin->get_plugin_name()] = clock::time_point{};
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
            udpFpsLimit = generalConfig.getUdpFpsLimit();
        }

        // Bulk UDP is only useful while the control connection is alive and
        // the matrix has explicitly reported that its canvas is enabled. Do
        // not even ask plugins to render/encode packets while streaming is
        // paused; this saves CPU in addition to network bandwidth.
        if (!streamState_.can_send_udp())
        {
            std::this_thread::sleep_for(FRAME_DURATION_MS);
            continue;
        }

        auto largePayloadMinInterval = std::chrono::duration<double, std::milli>(1000.0 / udpFpsLimit);

        for (auto &pl : plugins | std::views::values)
        {
            const auto &name = pl->get_plugin_name();

            if (pl->is_large_payload_plugin())
            {
                auto now = clock::now();
                auto &last = lastLargePayloadSend[name];
                if (now - last < largePayloadMinInterval)
                    continue;
            }

            auto packets = pl->compute_next_packets(scene);
            if (packets.empty())
                continue;

            lastLargePayloadSend[name] = clock::now();

            bool batch_ok = true;
            std::string batch_error;
            for (auto &packet : packets) {
                auto res = this->udpSender.sendPacket(std::move(packet), hostname, port);
                if (!res.has_value()) {
                    batch_ok = false;
                    batch_error = res.error();
                    break;
                }
            }
            if (!batch_ok)
            {
                std::unique_lock<std::mutex> lock(lastErrorMutex);
                lastError = std::move(batch_error);
                consecutiveError_++;

                if (consecutiveError_ < 3)
                    spdlog::error("Failed to send packet batch: {}", lastError);
            }
            else
            {
                std::unique_lock<std::mutex> lock(lastErrorMutex);
                lastError.clear();
                consecutiveError_ = 0;
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