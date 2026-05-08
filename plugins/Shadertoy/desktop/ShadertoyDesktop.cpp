#include "ShadertoyDesktop.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <shadertoy/PipelineEditor.hpp>
#include <shared/desktop/glfw.h>
#include "shared/desktop/utils.h"

#include "CanvasPacket.h"

extern "C" PLUGIN_EXPORT ShadertoyDesktop *createShadertoy()
{
    return new ShadertoyDesktop();
}

extern "C" PLUGIN_EXPORT void destroyShadertoy(ShadertoyDesktop *c)
{
    delete c;
}

ShadertoyDesktop::~ShadertoyDesktop()
{
    if (mCache)
    {
        mCache->save();
    }
}

static bool isActive = false;
static bool currShaderHasError = false;
void ShadertoyDesktop::after_swap(ImGuiContext *imCtx)
{
    if (currShaderHasError || !isActive)
        return;

    if (hasUrlChanged)
    {
        if (!shader_file_path.empty())
        {
            spdlog::info("Custom shader changed, loading local file {}...", shader_file_path);
            loadLocalShaderFromFile(shader_file_path);
        }
        else
        {
            spdlog::info("URL changed, loading shader {}...", url);
            loadCacheFromUrl(url);
        }
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

void ShadertoyDesktop::initialize_imgui(ImGuiContext *im_gui_context, ImGuiMemAllocFunc*alloc_fn,
    ImGuiMemFreeFunc*free_fn, void **user_data) {
    ImGui::SetCurrentContext(im_gui_context);
    ImGui::GetAllocatorFunctions(alloc_fn, free_fn, user_data);
}

void ShadertoyDesktop::render()
{
    if (!initError.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", initError.c_str());
        return;
    }

    if (!shader_file_path.empty())
    {
        ImGui::Text("Current Local Shader: %s", shader_file_path.c_str());
    }
    else if (url.empty())
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

    if (ImGui::Button("Manage Cache"))
        mShowCacheEditor = true;

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

    renderCacheEditorUI();
}

void ShadertoyDesktop::renderCacheEditorUI()
{
    if (!mShowCacheEditor)
        return;

    if (!ImGui::Begin("Shader Cache Manager", &mShowCacheEditor))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Add Custom Cache Entry");
    ImGui::InputText("Cache Key (URL)", mCacheKeyInput, sizeof(mCacheKeyInput));
    ImGui::InputTextMultiline("Cache Value (Response)", mCacheValueInput, sizeof(mCacheValueInput),
                              ImVec2(-1.0f, 200.0f));

    if (ImGui::Button("Add to Cache") && mCache)
    {
        if (strlen(mCacheKeyInput) > 0 && strlen(mCacheValueInput) > 0)
        {
            mCache->set(std::string(mCacheKeyInput), std::string(mCacheValueInput));
            mCacheKeyInput[0] = '\0';
            mCacheValueInput[0] = '\0';
            spdlog::info("Added custom cache entry");
        }
    }

    ImGui::Separator();
    ImGui::Text("Cached Entries: %zu", mCache ? mCache->getKeys().size() : 0);

    if (mCache && ImGui::BeginChild("CacheList", ImVec2(0, -50), true))
    {
        auto keys = mCache->getKeys();
        for (const auto& key : keys)
        {
            ImGui::PushID(key.c_str());
            ImGui::TextWrapped("%s", key.c_str());
            ImGui::SameLine();

            if (ImGui::SmallButton("Delete"))
            {
                mCacheToDelete = key;
            }

            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    // Handle deletion
    if (!mCacheToDelete.empty())
    {
        if (ImGui::BeginPopupModal("Delete Cache Entry?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Are you sure you want to delete this cache entry?\n%s", mCacheToDelete.c_str());
            ImGui::Separator();

            if (ImGui::Button("Delete", ImVec2(120, 0)))
            {
                if (mCache)
                {
                    mCache->remove(mCacheToDelete);
                    spdlog::info("Deleted cache entry: {}", mCacheToDelete);
                }
                mCacheToDelete.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                mCacheToDelete.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (!mCacheToDelete.empty())
            ImGui::OpenPopup("Delete Cache Entry?");
    }

    ImGui::End();
}

void ShadertoyDesktop::loadCacheFromUrl(const std::string& url)
{
    if (!mCache)
    {
        spdlog::error("Cache not initialized");
        return;
    }

    auto cached = mCache->get(url);
    if (cached.has_value())
    {
        spdlog::info("Loading shader from cache for {}", url);
        auto res = ShaderToy::PipelineEditor::get().loadFromShaderToyResponse(url, cached.value());
        if (!res.has_value())
        {
            spdlog::error("Failed to load from cache: {}", res.error().what());
            send_websocket_message("next_shader");
            currShaderHasError = true;
            return;
        }
        hasUrlChanged = false;
    }
    else
    {
        spdlog::info("Loading shader {} from ShaderToy...", url);
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
}

void ShadertoyDesktop::loadLocalShaderFromFile(const std::string &file_path)
{
    try
    {
        if (!std::filesystem::exists(file_path))
        {
            spdlog::error("Local shader file does not exist: {}", file_path);
            currShaderHasError = true;
            return;
        }

        std::ifstream file(file_path);
        if (!file.is_open())
        {
            spdlog::error("Failed to open local shader file: {}", file_path);
            currShaderHasError = true;
            return;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        const auto source = ss.str();
        const std::string uniforms = R"(uniform vec3 iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform int iFrame;
)";
        const std::string shader_code = uniforms + "\n" + source;

        nlohmann::json response = {
            {"Shader",
             {
                 {"info",
                  {
                      {"id", "local"},
                      {"name", std::filesystem::path(file_path).stem().string()},
                      {"username", "local"},
                      {"description", "Local custom shader"},
                  }},
                 {"renderpass",
                  nlohmann::json::array(
                      {{
                          {"name", "Image"},
                          {"type", "image"},
                          {"code", shader_code},
                          {"inputs", nlohmann::json::array()},
                          {"outputs", nlohmann::json::array({{{"id", "4dXGR8"}, {"channel", 0}}})},
                      }})},
             }},
        };

        auto res = ShaderToy::PipelineEditor::get().loadFromShaderToyResponse("local", response.dump());
        if (!res.has_value())
        {
            spdlog::error("Failed to load local shader '{}': {}", file_path, res.error().what());
            currShaderHasError = true;
            return;
        }

        hasUrlChanged = false;
        currShaderHasError = false;
    }
    catch (const std::exception &e)
    {
        spdlog::error("Error loading local shader '{}': {}", file_path, e.what());
        currShaderHasError = true;
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

        shader_file_path.clear();
        url = newUrl;
        hasUrlChanged = urlChanged;
        currShaderHasError = false;
    }
    else if (message.starts_with("shader_file:"))
    {
        const std::string filePath = message.substr(12);
        bool changed = filePath != shader_file_path;
        shader_file_path = filePath;
        url = "local://" + std::filesystem::path(filePath).filename().string();
        hasUrlChanged = changed;
        currShaderHasError = false;
    }
}

std::optional<std::unique_ptr<UdpPacket, void (*)(UdpPacket *)>> ShadertoyDesktop::compute_next_packet(
    const std::string sceneName)
{
    if ((sceneName != "shadertoy" && !sceneName.starts_with("custom_shader:")) || width == 0 || height == 0 || !initError.empty())
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
    
    // Initialize cache with plugin directory
    auto cacheDir = get_data_dir() / "cache";
    mCache = std::make_unique<ShaderCache>(cacheDir);
}
