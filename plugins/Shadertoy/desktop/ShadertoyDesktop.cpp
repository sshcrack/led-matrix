#include "ShadertoyDesktop.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <shadertoy/NodeEditor/PipelineEditor.hpp>
#include <shared/desktop/glfw.h>

#include "CanvasPacket.h"

extern "C" PLUGIN_EXPORT ShadertoyDesktop *createShadertoy()
{
    return new ShadertoyDesktop();
}

extern "C" PLUGIN_EXPORT void destroyShadertoy(ShadertoyDesktop *c)
{
    delete c;
}

void ShadertoyDesktop::render(ImGuiContext *imGuiCtx)
{
    ImGui::SetCurrentContext(imGuiCtx);

    if (!initError.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", initError.c_str());
        return;
    }

    if (url.empty())
    {
        ImGui::Text("Currently no URL is set (or no shadertoy scene is active).");
    }
    else
    {
        ImGui::TextLinkOpenURL(("Current URL: " + url).c_str(), url.c_str());
    }
    ImGui::Text("Canvas Size: %dx%d", width, height);

    if (hasUrlChanged)
    {
        spdlog::info("Loading shader {}...", url);
        ShaderToy::PipelineEditor::get().loadFromShaderToy(url);
        hasUrlChanged = false;
    }

    ShaderToy::PipelineEditor::get().update(ctx);
    ctx.tick(60);

    const std::vector<uint8_t> data = ctx.renderToBuffer(ImVec2(width, height));
    std::unique_lock lock(currDataMutex);
    currData = data;
/*
    if (!ImGui::Begin("Canvas", nullptr)) {
        ImGui::End();
        return;
    }

    const auto reservedHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("CanvasRegion", ImVec2(0, -reservedHeight), false)) {
        ImVec2 size(width, height);

        const auto base = ImGui::GetCursorScreenPos();
        std::optional<ImVec4> mouse = std::nullopt;

        ctx.render(base, size, mouse);
        ImGui::EndChild();
    }

    ImGui::End();
*/
}

void ShadertoyDesktop::on_websocket_message(const std::string message)
{
    if (message.starts_with("size:"))
    {
        std::string sizeStr = message.substr(5);
        const auto xPos = sizeStr.find('x');

        width = std::stoi(sizeStr.substr(0, xPos));
        height = std::stoi(sizeStr.substr(xPos + 1));
    }

    if (message.starts_with("url:"))
    {
        url = message.substr(4);
        hasUrlChanged = true;
    }
}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> ShadertoyDesktop::compute_next_packet(
    const std::string sceneName)
{
    if (sceneName != "shadertoy" || width == 0 || height == 0 || !initError.empty())
    {
        return std::nullopt; // Not for this scene
    }

    std::shared_lock lock(currDataMutex);
    return std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>(new CanvasPacket(currData),
                                                             [](UdpPacket *packet)
                                                             {
                                                                 delete dynamic_cast<CanvasPacket *>(packet);
                                                             });
}

void ShadertoyDesktop::post_init()
{
    auto init = glewInit();
    if (init != GLEW_OK)
    {
        initError = "Failed to initialize glew: " + std::string(
                                                        reinterpret_cast<const char *>(glewGetErrorString(init)));
        spdlog::error(initError);
    }
    else
        spdlog::info("Glew initialized successfully");
}
