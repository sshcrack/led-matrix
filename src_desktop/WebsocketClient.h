#pragma once
#include <ixwebsocket/IXWebSocket.h>
#include "UdpSender.h"
#include <string>
#include <thread>
#include <mutex>
#include <spdlog/spdlog.h>

class WebsocketClient
{
public:
    WebsocketClient();
    ~WebsocketClient();

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

    void start()
    {
        spdlog::info("Starting WebSocket client");
        webSocket.start();
    }

    void stop() {
        spdlog::info("Stopping WebSocket client");
        webSocket.close();
    }

private:
    ix::WebSocket webSocket;
    UdpSender udpSender;

    std::thread senderThread;

    std::mutex activeSceneMutex;
    std::string activeScene = "";

    void threadLoop();

    bool serverRunning = true;
};