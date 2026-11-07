#include "plugins/Video/matrix/VideoPlugin.h"
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/canvas_consts.h>
#include <emulator.h>
#include <algorithm>
#include <iostream>

int main() {
    auto* manager = Plugins::PluginManager::instance();
    manager->initialize();
    VideoPlugin* plugin = nullptr;
    for (auto* p : manager->get_plugins())
        if (p->get_plugin_name() == "Video") plugin = dynamic_cast<VideoPlugin*>(p);
    if (!plugin) { std::cerr << "Video plugin not loaded\n"; return 1; }
    std::vector<std::string> commands;
    Plugins::BasicPlugin::set_message_sink([&](const auto&, const auto& message) { commands.push_back(message); });
    auto make_scene = [&](const std::string& url) {
        auto scene = plugin->get_scenes().front()->create();
        scene->update_default_properties();
        scene->register_properties();
        scene->load_properties(nlohmann::json{{"video_urls", std::vector<std::string>{url}}});
        scene->initialize(32, 32);
        return scene;
    };
    rgb_matrix::RGBMatrix::Options options;
    options.rows = options.cols = 32;
    options.chain_length = options.parallel = 1;
    rgb_matrix::EmulatorOptions emulator;
    emulator.headless = true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(options, emulator));
    if (!matrix) { std::cerr << "Cannot create headless matrix\n"; return 1; }
    auto* canvas = matrix->CreateFrameCanvas();
    Constants::width = Constants::height = 32;
    auto first = make_scene("https://example.invalid/A");
    auto next = make_scene("https://example.invalid/B");
    first->render(canvas);
    plugin->on_websocket_message("status:downloading");
    first->after_render_stop();
    // A reply already queued by the old downloader arrives after the stop.
    plugin->on_websocket_message("status:downloading");
    commands.clear();
    next->render(canvas);
    const bool switched = std::find(commands.begin(), commands.end(), "url:https://example.invalid/B") != commands.end();
    if (!switched) std::cerr << "FAIL: delayed status from A prevents B from selecting its URL\n";
    next->after_render_stop();
    first->render(canvas);
    plugin->on_websocket_message("status:playing");
    const std::vector<uint8_t> old_pixels(32 * 32 * 3, 42);
    plugin->on_udp_packet(0x03, old_pixels.data(), old_pixels.size());
    first->render(canvas);
    commands.clear();
    // Transitions prepare the incoming scene before stopping the outgoing one.
    next->render(canvas);
    bool overlap_ok = std::find(commands.begin(), commands.end(), "url:https://example.invalid/B") != commands.end();
    if (!overlap_ok) std::cerr << "FAIL: incoming video inherits outgoing scene's selected URL\n";
    uint8_t r = 0, g = 0, b = 0;
    canvas->GetPixel(16, 16, &r, &g, &b);
    bool pixels_ok = r == 0 && g == 0 && b == 0;
    if (!pixels_ok) std::cerr << "FAIL: incoming scene displays stale outgoing pixels while loading\n";
    const std::vector<uint8_t> new_pixels(32 * 32 * 3, 180);
    plugin->on_udp_packet(0x03, new_pixels.data(), new_pixels.size());
    plugin->on_websocket_message("status:playing");
    next->render(canvas);
    canvas->GetPixel(16, 16, &r, &g, &b);
    pixels_ok = pixels_ok && r == 180 && g == 180 && b == 180;
    if (!pixels_ok) std::cerr << "FAIL: incoming scene did not render replacement video pixels\n";
    commands.clear();
    first->after_render_stop();
    if (std::find(commands.begin(), commands.end(), "stream:stop") != commands.end()) {
        std::cerr << "FAIL: outgoing scene stops the incoming video's stream\n";
        overlap_ok = false;
    }
    const auto replay = plugin->on_websocket_open();
    const bool replay_ok = replay && std::find(replay->begin(), replay->end(), "stream:start") != replay->end()
        && std::find(replay->begin(), replay->end(), "url:https://example.invalid/B") != replay->end();
    if (!replay_ok) std::cerr << "FAIL: reconnecting desktop is not told which video to resume\n";
    next->after_render_stop();
    const auto stopped_replay = plugin->on_websocket_open();
    const bool stopped_ok = stopped_replay && std::none_of(stopped_replay->begin(), stopped_replay->end(),
        [](const auto& message) { return message.starts_with("url:"); });
    if (!stopped_ok) std::cerr << "FAIL: stopped stream is replayed on reconnect\n";
    Plugins::BasicPlugin::set_message_sink({});
    return switched && overlap_ok && pixels_ok && replay_ok && stopped_ok ? 0 : 1;
}
