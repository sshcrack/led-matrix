#include "shared/desktop/plugin/main.h"
#include "shared/desktop/WebsocketClient.h"
#include <spdlog/spdlog.h>

void Plugins::DesktopPlugin::send_websocket_message(const std::string &message)
{
    if (WebsocketClient::instance() == nullptr)
    {
        spdlog::warn("WebsocketClient instance is null, cannot send message: {}", message);
        return;
    }

    WebsocketClient::instance()->webSocket.send("msg:" + get_plugin_name() + ":" + message);
}