#include "shared/matrix/plugin/main.h"
#include <mutex>

namespace {
std::mutex sink_mutex;
Plugins::BasicPlugin::MessageSink message_sink;
}

void Plugins::BasicPlugin::set_message_sink(MessageSink sink) {
    std::lock_guard lock(sink_mutex);
    message_sink = std::move(sink);
}

bool Plugins::BasicPlugin::deliver_to_message_sink(const std::string& plugin, const std::string& message) {
    MessageSink sink;
    {
        std::lock_guard lock(sink_mutex);
        sink = message_sink;
    }
    if (!sink) return false;
    sink(plugin, message);
    return true;
}
