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

static bool isActive = false;
static bool currShaderHasError = false;
void ShadertoyDesktop::after_swap(ImGuiContext *imCtx)
{
    if (currShaderHasError || !isActive)
        return;

    if (hasUrlChanged)
    {
        spdlog::info("Loading shader {}...", url);
        auto res = ShaderToy::PipelineEditor::get().loadFromShaderToy(url);
        if (!res.has_value())
        {
            spdlog::error("Failed to load from shadertoy: {}", res.error().what());

            send_websocket_message("next_shader");
            currShaderHasError = true;
            return;
        }
        hasUrlChanged = false;
    }

    auto res = ShaderToy::PipelineEditor::get().update(ctx);
    if (!res.has_value())
    {
        spdlog::error("Failed to update shader: {}", res.error().what());

        send_websocket_message("next_shader");
        currShaderHasError = true;
        return;
    }

    ctx.tick(60);

    const std::vector<uint8_t> data = ctx.renderToBuffer(ImVec2(width, height), imCtx);

    std::unique_lock lock(currDataMutex);
    currData = data;
}

void ShadertoyDesktop::render()
{
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
        ImGui::Text("Compiling shader...");

    if (ImGui::Button("Next Shader"))
        send_websocket_message("next_shader");

    ImGui::Checkbox("Enable Preview", &enablePreview);
    if (enablePreview)
    {
        if (!ImGui::Begin("Canvas", nullptr))
        {
            ImGui::End();
            return;
        }

        const auto reservedHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("CanvasRegion", ImVec2(0, -reservedHeight), false))
        {
            ImVec2 size(width, height);

            const auto base = ImGui::GetCursorScreenPos();
            std::optional<ImVec4> mouse = std::nullopt;

            // TODO rendering twice may cause issues for lighting and stuff
            ctx.render(base, size, mouse);
            ImGui::EndChild();
        }

        ImGui::End();
    }
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
        std::string newUrl = message.substr(4);
        bool urlChanged = newUrl != url;

        url = newUrl;
        hasUrlChanged = urlChanged;
        currShaderHasError = false;
    }
}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> ShadertoyDesktop::compute_next_packet(
    const std::string sceneName)
{
    if (sceneName != "shadertoy" || width == 0 || height == 0 || !initError.empty())
    {
        isActive = false;
        return std::nullopt; // Not for this scene
    }

    isActive = true;
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
