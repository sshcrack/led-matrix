#include <shared/desktop/WebsocketClient.h>
#include <shared/desktop/plugin_loader/loader.h>

#include <ixwebsocket/IXWebSocketServer.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;

#ifndef _WIN32
namespace {
void write_executable(const fs::path& path, const std::string& body)
{
    std::ofstream out(path);
    out << body;
    out.close();
    chmod(path.c_str(), 0755);
}
}
#endif

int main()
{
#ifdef _WIN32
    return 0;
#else
    const auto root = fs::temp_directory_path() /
        ("led-matrix-ws-dispatch-" + std::to_string(static_cast<long long>(::getpid())));
    const auto bin = root / "bin";
    const auto home = root / "home";
    fs::create_directories(bin);
    fs::create_directories(home);

    write_executable(bin / "yt-dlp", R"SH(#!/bin/sh
if [ "$1" = "--version" ]; then echo fake-yt-dlp; exit 0; fi
case "$*" in
  *ytsearch1:*) sleep 3; echo 'https://www.youtube.com/watch?v=fake'; exit 0 ;;
  *) echo 180; exit 0 ;;
esac
)SH");
    write_executable(bin / "ffmpeg", R"SH(#!/bin/sh
if [ "$1" = "-version" ]; then echo fake-ffmpeg; exit 0; fi
exit 0
)SH");

    const char* old_path = std::getenv("PATH");
    const std::string path = bin.string() + ":" + (old_path ? old_path : "");
    setenv("PATH", path.c_str(), 1);
    setenv("HOME", home.c_str(), 1);

    auto* manager = Plugins::PluginManager::instance();
    manager->initialize();
    bool found_spotify_mv = false;
    for (const auto& [name, plugin] : manager->get_plugins()) {
        if (name == "SpotifyMV" || plugin->get_plugin_name() == "SpotifyMV") {
            found_spotify_mv = true;
            plugin->post_init();
        }
    }
    if (!found_spotify_mv) {
        std::cerr << "SpotifyMV desktop plugin not loaded from PLUGIN_DIR\n";
        return 2;
    }

    const int server_port = 19000 + (static_cast<int>(::getpid()) % 10000);
    ix::WebSocketServer server(server_port, "127.0.0.1");
    server.setOnClientMessageCallback([](auto, auto&, const auto&) {});
    if (!server.listenAndStart()) {
        std::cerr << "failed to start local WebSocket server\n";
        return 3;
    }

    auto client = WebsocketClient::create();
    WebsocketClient::setInstance(client.get());
    client->setUrl("ws://127.0.0.1:" + std::to_string(server_port) + "/desktopWebsocket");
    client->start();
    if (!client->isTransportStarted()) {
        std::cerr << "initial application start did not mark the transport as running\n";
        return 7;
    }
    if (!client->webSocket.isAutomaticReconnectionEnabled()) {
        std::cerr << "automatic reconnection was not enabled by the initial application start\n";
        return 6;
    }

    const auto connect_deadline = std::chrono::steady_clock::now() + 2s;
    while (client->getReadyState() != ix::ReadyState::Open
           && std::chrono::steady_clock::now() < connect_deadline)
        std::this_thread::sleep_for(10ms);
    if (client->getReadyState() != ix::ReadyState::Open) {
        std::cerr << "client did not connect to local WebSocket server\n";
        return 4;
    }

    std::shared_ptr<ix::WebSocket> peer;
    const auto peer_deadline = std::chrono::steady_clock::now() + 2s;
    while (!peer && std::chrono::steady_clock::now() < peer_deadline) {
        const auto clients = server.getClients();
        if (!clients.empty()) peer = *clients.begin();
        else std::this_thread::sleep_for(10ms);
    }
    if (!peer) {
        std::cerr << "server did not observe connected client\n";
        return 5;
    }

    peer->send("msg:SpotifyMV:track:track-a:Song\nArtist\nofficial music video\ntrue\n0\n180000");
    std::this_thread::sleep_for(100ms);

    // This stop intentionally makes SpotifyMV cancel/join an in-flight search.
    // The WebSocket transport callback must not perform that work inline.
    peer->send("msg:SpotifyMV:stop");
    peer->send("active:metaball");

    const auto active_deadline = std::chrono::steady_clock::now() + 500ms;
    while (client->getActiveScene() != "metaball"
           && std::chrono::steady_clock::now() < active_deadline)
        std::this_thread::sleep_for(5ms);

    const bool responsive = client->getActiveScene() == "metaball";
    if (!responsive) {
        std::cerr << "WebSocket control callback was blocked by slow SpotifyMV stop\n";
        return 1;
    }

    // Drop the connection from the server side while leaving the listener up.
    // The initial application start (not a later UI click) must have armed
    // automatic reconnection and establish a fresh peer on its own.
    peer->close(1001, "reconnect regression test");
    bool reconnected = false;
    const auto reconnect_deadline = std::chrono::steady_clock::now() + 4s;
    while (std::chrono::steady_clock::now() < reconnect_deadline) {
        const auto clients = server.getClients();
        for (const auto& candidate : clients) {
            if (candidate != peer && client->getReadyState() == ix::ReadyState::Open) {
                reconnected = true;
                break;
            }
        }
        if (reconnected) break;
        std::this_thread::sleep_for(20ms);
    }
    if (!reconnected) {
        std::cerr << "initial application connection did not automatically reconnect after a drop\n";
        return 9;
    }

    client->stop();
    if (client->isTransportStarted()) {
        std::cerr << "explicit Disconnect did not stop the reconnect lifecycle\n";
        return 8;
    }
    WebsocketClient::setInstance(nullptr);
    client.reset();
    server.stop();
    manager->destroy_plugins();
    fs::remove_all(root);

    std::cout << "WebSocket stayed responsive and automatically reconnected after a forced drop\n";
    return 0;
#endif
}
