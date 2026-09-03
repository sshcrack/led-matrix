#pragma once
#include "shared/desktop/macro.h"
#include <ixwebsocket/IXWebSocket.h>
#include "shared/desktop/UdpSender.h"
#include "shared/desktop/DesktopStreamState.h"
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <deque>
#include <unordered_map>
#include <spdlog/spdlog.h>

class SHARED_DESKTOP_API WebsocketClient : public std::enable_shared_from_this<WebsocketClient>
{
public:
    static std::shared_ptr<WebsocketClient> create();
    ~WebsocketClient();

    static WebsocketClient *instance();

    static void setInstance(WebsocketClient *instance);

    static std::atomic<int>& net_refs()
    {
        static std::atomic<int> refs{0};
        return refs;
    }

    ix::ReadyState getReadyState() const
    {
        return webSocket.getReadyState();
    }

    std::string getActiveScene()
    {
        std::unique_lock<std::mutex> lock(activeSceneMutex);
        return activeScene;
    }

    std::string getReadyStateString() const
    {
        switch (webSocket.getReadyState())
        {
        case ix::ReadyState::Connecting:
            return "Connecting";
        case ix::ReadyState::Open:
            return "Open";
        case ix::ReadyState::Closing:
            return "Closing";
        case ix::ReadyState::Closed:
            return "Closed";
        default:
            return "Unknown";
        };
    }

    void setUrl(const std::string &url)
    {
        spdlog::info("Setting WebSocket URL to: {}", url);
        webSocket.setUrl(url);
    }

    void setup_callback();

    void start()
    {
        // `ReadyState::Closed` is also used between automatic-reconnect
        // attempts, so it cannot tell us whether the transport lifecycle has
        // actually been stopped by the user. Keep that state explicitly.
        if (transportStarted_.exchange(true))
            return;

        spdlog::info("Starting WebSocket client");
        // Reconnection is a transport policy, not a UI-button side effect.
        // Enable it before starting so the initial application connection is
        // protected too (previously only a later manual Connect click did it).
        webSocket.enableAutomaticReconnection();
        webSocket.start();

        if (!senderRunning.exchange(true))
        {
            senderThread = std::thread(&WebsocketClient::threadLoop, this);
        }
    }

    void stop()
    {
        transportStarted_.store(false);
        spdlog::info("Stopping WebSocket client");
        webSocket.disableAutomaticReconnection();
        webSocket.stop();
        if (senderRunning.exchange(false))
        {
            if (senderThread.joinable())
            {
                senderThread.join();
            }
        }
    }

    std::string getLastError()
    {
        std::unique_lock<std::mutex> lock(lastErrorMutex);
        return lastError;
    }

    [[nodiscard]] bool isTransportStarted() const
    {
        return transportStarted_.load();
    }

    ix::WebSocket webSocket;

private:
    WebsocketClient();
    UdpSender udpSender;
    DesktopStreamState streamState_;

    std::thread senderThread;

    std::mutex activeSceneMutex;
    std::string activeScene = "";

    std::mutex lastErrorMutex;
    std::string lastError = "";

    void threadLoop();
    void enqueuePluginMessage(std::string plugin_name, std::string message);
    void pluginMessageLoop();

    std::atomic<bool> senderRunning{false};
    std::atomic<bool> transportStarted_{false};
    int consecutiveError_ = 0;

    // Plugin callbacks are deliberately kept off ixwebsocket's network
    // callback thread. Some plugins stop child processes / join decoder or
    // search threads, which can legitimately take hundreds of milliseconds.
    // Blocking the network thread there can starve ping/pong/reconnect work and
    // make an unrelated scene/mode switch look like a WebSocket disconnect.
    std::mutex pluginMessageMutex_;
    std::condition_variable pluginMessageCv_;
    std::deque<std::pair<std::string, std::string>> pluginMessages_;
    std::atomic<bool> pluginMessageRunning_{true};
    std::thread pluginMessageThread_;
};

SHARED_DESKTOP_API extern WebsocketClient *websocketClientInstance;