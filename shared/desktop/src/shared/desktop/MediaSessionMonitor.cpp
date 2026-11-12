#include "shared/desktop/MediaSessionMonitor.h"

#include <spdlog/spdlog.h>

#if defined(_WIN32) && __has_include(<winrt/Windows.Media.Control.h>)
#define LED_MATRIX_MEDIA_GSMTC 1
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#elif defined(__linux__)
#define LED_MATRIX_MEDIA_MPRIS 1
#include <dbus/dbus.h>

#include <string_view>
#include <vector>
#endif

namespace {
#if defined(LED_MATRIX_MEDIA_MPRIS)
constexpr std::string_view MprisPrefix = "org.mpris.MediaPlayer2.";
constexpr int CallTimeoutMs = 500;

/// One private session-bus connection, used only by the monitor thread.
class MprisProbe {
public:
    ~MprisProbe() { disconnect(); }

    MediaSessionMonitor::Snapshot poll()
    {
        MediaSessionMonitor::Snapshot result;
        if (!ensure_connected())
            return result;
        result.available = true;
        for (const auto& name : player_names()) {
            if (playback_status(name) != "Playing")
                continue;
            result.playing = true;
            // "org.mpris.MediaPlayer2.firefox.instance_1_42" -> "firefox"
            auto player = name.substr(MprisPrefix.size());
            result.player = player.substr(0, player.find('.'));
            break;
        }
        return result;
    }

private:
    bool ensure_connected()
    {
        if (conn_ && dbus_connection_get_is_connected(conn_))
            return true;
        disconnect();
        DBusError err;
        dbus_error_init(&err);
        conn_ = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
            if (!warned_)
                spdlog::debug("Media session monitor: no D-Bus session bus ({})", err.message);
            warned_ = true;
            dbus_error_free(&err);
        }
        if (!conn_)
            return false;
        dbus_connection_set_exit_on_disconnect(conn_, false);
        return true;
    }

    void disconnect()
    {
        if (!conn_)
            return;
        dbus_connection_close(conn_);
        dbus_connection_unref(conn_);
        conn_ = nullptr;
    }

    DBusMessage* call(DBusMessage* msg)
    {
        if (!msg)
            return nullptr;
        DBusError err;
        dbus_error_init(&err);
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn_, msg, CallTimeoutMs, &err);
        dbus_message_unref(msg);
        if (dbus_error_is_set(&err))
            dbus_error_free(&err);
        return reply;
    }

    std::vector<std::string> player_names()
    {
        std::vector<std::string> names;
        DBusMessage* reply =
            call(dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames"));
        if (!reply)
            return names;
        DBusMessageIter args;
        DBusMessageIter array;
        if (dbus_message_iter_init(reply, &args) && dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_ARRAY) {
            dbus_message_iter_recurse(&args, &array);
            while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRING) {
                const char* name = nullptr;
                dbus_message_iter_get_basic(&array, &name);
                if (name && std::string_view(name).starts_with(MprisPrefix))
                    names.emplace_back(name);
                dbus_message_iter_next(&array);
            }
        }
        dbus_message_unref(reply);
        return names;
    }

    std::string playback_status(const std::string& bus_name)
    {
        DBusMessage* msg =
            dbus_message_new_method_call(bus_name.c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get");
        if (!msg)
            return {};
        const char* iface = "org.mpris.MediaPlayer2.Player";
        const char* prop = "PlaybackStatus";
        dbus_message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
        DBusMessage* reply = call(msg);
        if (!reply)
            return {};
        std::string status;
        DBusMessageIter args;
        DBusMessageIter variant;
        if (dbus_message_iter_init(reply, &args) && dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
            dbus_message_iter_recurse(&args, &variant);
            if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_STRING) {
                const char* value = nullptr;
                dbus_message_iter_get_basic(&variant, &value);
                if (value)
                    status = value;
            }
        }
        dbus_message_unref(reply);
        return status;
    }

    DBusConnection* conn_ = nullptr;
    bool warned_ = false;
};
#elif defined(LED_MATRIX_MEDIA_GSMTC)
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;

class GsmtcProbe {
public:
    GsmtcProbe()
    {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
        }
        catch (...) {
            // Already initialised on this thread: fine, it is our own thread.
        }
    }

    MediaSessionMonitor::Snapshot poll()
    {
        MediaSessionMonitor::Snapshot result;
        try {
            if (!manager_)
                manager_ = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            if (!manager_)
                return result;
            result.available = true;
            for (const auto& session : manager_.GetSessions()) {
                const auto info = session.GetPlaybackInfo();
                if (!info || info.PlaybackStatus() != GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
                    continue;
                result.playing = true;
                // "Spotify.exe" / "Microsoft.ZuneMusic_8wekyb3d8bbwe!Microsoft.ZuneMusic"
                result.player = winrt::to_string(session.SourceAppUserModelId());
                if (const auto bang = result.player.find('!'); bang != std::string::npos)
                    result.player = result.player.substr(0, bang);
                if (result.player.ends_with(".exe"))
                    result.player.resize(result.player.size() - 4);
                break;
            }
        }
        catch (const winrt::hresult_error& e) {
            if (!warned_)
                spdlog::debug("Media session monitor: {}", winrt::to_string(e.message()));
            warned_ = true;
            manager_ = nullptr;
            result = {};
        }
        catch (...) {
            manager_ = nullptr;
            result = {};
        }
        return result;
    }

private:
    GlobalSystemMediaTransportControlsSessionManager manager_{nullptr};
    bool warned_ = false;
};
#else
class NullProbe {
public:
    MediaSessionMonitor::Snapshot poll() { return {}; }
};
#endif

#if defined(LED_MATRIX_MEDIA_MPRIS)
using Probe = MprisProbe;
#elif defined(LED_MATRIX_MEDIA_GSMTC)
using Probe = GsmtcProbe;
#else
using Probe = NullProbe;
#endif
}  // namespace

MediaSessionMonitor::MediaSessionMonitor(const std::chrono::milliseconds interval) : interval_(interval) {}

MediaSessionMonitor::~MediaSessionMonitor()
{
    stop();
}

void MediaSessionMonitor::start()
{
    std::lock_guard lock(mutex_);
    if (thread_.joinable())
        return;
    stopping_ = false;
    thread_ = std::thread([this] { run(); });
}

void MediaSessionMonitor::stop()
{
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable())
        thread_.join();
    std::lock_guard lock(mutex_);
    snapshot_ = {};
}

MediaSessionMonitor::Snapshot MediaSessionMonitor::snapshot() const
{
    std::lock_guard lock(mutex_);
    return snapshot_;
}

void MediaSessionMonitor::run()
{
    Probe probe;
    std::unique_lock lock(mutex_);
    while (!stopping_) {
        lock.unlock();
        auto next = probe.poll();
        lock.lock();
        if (stopping_)
            break;
        snapshot_ = std::move(next);
        wake_.wait_for(lock, interval_, [this] { return stopping_; });
    }
}
