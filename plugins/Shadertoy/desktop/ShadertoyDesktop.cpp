#include "ShadertoyDesktop.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <shadertoy/PipelineEditor.hpp>
#include <shared/desktop/glfw.h>
#include <nfd.hpp>
#include <shared/desktop/utils.h>

#include "CanvasPacket.h"

extern "C" PLUGIN_EXPORT ShadertoyDesktop *createShadertoy()
{
    return new ShadertoyDesktop();
}

extern "C" PLUGIN_EXPORT void destroyShadertoy(ShadertoyDesktop *c)
{
    delete c;
}

static std::string normalizeShaderToyUrl(const std::string& input)
{
    // Check if input is already a full URL
    if (input.find("http://") == 0 || input.find("https://") == 0)
    {
        return input;
    }
    
    // Check if input looks like a shader ID (alphanumeric, typically 6 characters)
    // Pattern: /view/XXXXXX where X is alphanumeric
    bool isLikelyId = true;
    for (char c : input)
    {
        if (!std::isalnum(c))
        {
            isLikelyId = false;
            break;
        }
    }
    
    if (isLikelyId && input.length() > 0)
    {
        return "https://www.shadertoy.com/view/" + input;
    }
    
    // Return as-is if doesn't match patterns
    return input;
}

ShadertoyDesktop::~ShadertoyDesktop()
{
    // Cache now saves individual files automatically
}

static bool isActive = false;
static bool currShaderHasError = false;
void ShadertoyDesktop::after_swap(ImGuiContext *imCtx)
{
    if (currShaderHasError || !isActive)
        return;

    if (hasUrlChanged)
    {
        spdlog::info("URL changed, loading shader {}...", url);
        loadCacheFromUrl(url);
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

    ImGui::Text("Add Cache Entry from File");
    ImGui::InputText("Cache Key (URL)", mCacheKeyInput, sizeof(mCacheKeyInput));
    ImGui::InputText("Cache File Path##file", mCacheFileInput, sizeof(mCacheFileInput));
    
    ImGui::SameLine();
    if (ImGui::Button("Browse...##cache"))
    {
        nfdchar_t* outPath = nullptr;
        nfdfilteritem_t filters[2] = { { "JSON files", "json" }, { "All files", nullptr } };
        nfdresult_t result = NFD_OpenDialog(&outPath, filters, 2, nullptr);
        if (result == NFD_OKAY)
        {
            strncpy(mCacheFileInput, outPath, sizeof(mCacheFileInput) - 1);
            mCacheFileInput[sizeof(mCacheFileInput) - 1] = '\0';
            NFD_FreePath(outPath);
        }
        else if (result == NFD_CANCEL)
        {
            // User cancelled, no action needed
        }
        else
        {
            spdlog::error("File dialog error: {}", NFD_GetError());
        }
    }

    if (ImGui::Button("Add to Cache") && mCache)
    {
        if (strlen(mCacheKeyInput) > 0 && strlen(mCacheFileInput) > 0)
        {
            std::string normalizedKey = normalizeShaderToyUrl(std::string(mCacheKeyInput));
            std::filesystem::path filePath(mCacheFileInput);
            mCache->setFromFile(normalizedKey, filePath);
            mCacheKeyInput[0] = '\0';
            mCacheFileInput[0] = '\0';
            spdlog::info("Added custom cache entry from file: {}", normalizedKey);
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
    
    // Initialize cache in writable data directory
    auto cacheDir = get_data_dir() / "cache" / "shadertoy";
    mCache = std::make_unique<ShaderCache>(cacheDir);
}
