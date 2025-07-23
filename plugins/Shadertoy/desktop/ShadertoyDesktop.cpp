#include "ShadertoyDesktop.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "CanvasPacket.h"

extern "C" PLUGIN_EXPORT ShadertoyDesktop *createShadertoy() {
    return new ShadertoyDesktop();
}

extern "C" PLUGIN_EXPORT void destroyShadertoy(ShadertoyDesktop *c) {
    delete c;
}

void ShadertoyDesktop::render(ImGuiContext *ctx) {
    ImGui::SetCurrentContext(ctx);

    if (url.empty()) {
        ImGui::Text("Currently no URL is set (or no shadertoy scene is active).");
    } else {
        ImGui::TextLinkOpenURL(("Current URL: " + url).c_str(), url.c_str());
    }
    ImGui::Text("Canvas Size: %dx%d", width, height);
}

void ShadertoyDesktop::on_websocket_message(const std::string message) {
    if (message.starts_with("size:")) {
        std::string sizeStr = message.substr(5);
        const auto xPos = sizeStr.find('x');

        width = std::stoi(sizeStr.substr(0, xPos));
        height = std::stoi(sizeStr.substr(xPos + 1));
    }

    if (message.starts_with("url:"))
        url = message.substr(4);
}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)> > ShadertoyDesktop::compute_next_packet(
    const std::string sceneName) {
    if (sceneName != "shadertoy" || width == 0 || height == 0) {
        return std::nullopt; // Not for this scene
    }

    std::vector<uint8_t> data;
    // Animate using a time-based value (e.g., sine wave)
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(now - start).count();

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            // Animate colors with sine waves based on time and position
            uint8_t r = static_cast<uint8_t>(127.5f * (1.0f + std::sin(t + x * 0.1f)));
            uint8_t g = static_cast<uint8_t>(127.5f * (1.0f + std::sin(t + y * 0.1f)));
            uint8_t b = static_cast<uint8_t>(127.5f * (1.0f + std::sin(t + (x + y) * 0.05f)));
            data.push_back(r);
            data.push_back(g);
            data.push_back(b);
        }
    }

    return std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>(new CanvasPacket(data),
                                                             [](UdpPacket *packet) {
                                                                 delete static_cast<CanvasPacket *>(packet);
                                                             });
}
