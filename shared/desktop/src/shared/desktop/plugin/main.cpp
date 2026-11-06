#include "shared/desktop/plugin/main.h"
#include "shared/desktop/WebsocketClient.h"
#include <mutex>
#include <spdlog/spdlog.h>

namespace
{
    std::mutex sink_mutex;
    Plugins::DesktopPlugin::MessageSink message_sink;
}

void Plugins::DesktopPlugin::set_message_sink(MessageSink sink)
{
    std::lock_guard lock(sink_mutex);
    message_sink = std::move(sink);
}

void Plugins::DesktopPlugin::send_websocket_message(const std::string &message)
{
    {
        std::lock_guard lock(sink_mutex);
        if (message_sink)
        {
            message_sink(get_plugin_name(), message);
            return;
        }
    }

    if (WebsocketClient::instance() == nullptr)
    {
        spdlog::warn("WebsocketClient instance is null, cannot send message: {}", message);
        return;
    }

    WebsocketClient::instance()->webSocket.send("msg:" + get_plugin_name() + ":" + message);
}
