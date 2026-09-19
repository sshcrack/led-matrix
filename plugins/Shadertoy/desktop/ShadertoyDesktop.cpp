#include "ShadertoyDesktop.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <shared/desktop/glfw.h>
#include <shared/desktop/audio_state.h>
#include "shared/desktop/utils.h"

#include "CanvasPacket.h"

REGISTER_PLUGIN(Shadertoy, ShadertoyDesktop)

ShadertoyDesktop::~ShadertoyDesktop()
{
    if (mCache)
    {
        mCache->save();
    }
}

static std::atomic_bool isActive{false};
static std::atomic_bool currShaderHasError{false};

namespace {
std::string shaderIdFromUrl(std::string_view value)
{
    if (const auto query = value.find_first_of("?#"); query != std::string_view::npos)
        value = value.substr(0, query);
    while (!value.empty() && value.back() == '/')
        value.remove_suffix(1);
    if (const auto slash = value.find_last_of('/'); slash != std::string_view::npos)
        value = value.substr(slash + 1);
    return std::string(value);
}

ShaderToy::AudioInput currentAudioInput() {
    const auto snapshot = DesktopAudioState::snapshot();
    ShaderToy::AudioInput audio;
    if (!snapshot.fresh())
        return audio;

    audio.available = true;
    audio.silence = snapshot.event(AudioProtocol::Silent) ||
                    snapshot.feature(AudioProtocol::Feature::Silence) > 0.5f;
    audio.sampleRate = snapshot.sample_rate;
    audio.spectrum = snapshot.spectrum;
    audio.waveform = snapshot.waveform;

    audio.loudness = snapshot.feature(AudioProtocol::Feature::LoudnessFast);
    audio.bass = 0.55f * snapshot.feature(AudioProtocol::Feature::Bass) +
                 0.45f * snapshot.feature(AudioProtocol::Feature::SubBass);
    audio.mid = snapshot.feature(AudioProtocol::Feature::Mid);
    audio.treble = snapshot.feature(AudioProtocol::Feature::Treble);

    audio.onset = snapshot.feature(AudioProtocol::Feature::OnsetStrength);
    audio.kick = snapshot.feature(AudioProtocol::Feature::Kick);
    audio.snare = snapshot.feature(AudioProtocol::Feature::Snare);
    audio.hihat = snapshot.feature(AudioProtocol::Feature::Hihat);

    audio.bpm = snapshot.feature(AudioProtocol::Feature::Bpm);
    audio.beatPhase = snapshot.feature(AudioProtocol::Feature::BeatPhase);
    audio.beatConfidence = snapshot.feature(AudioProtocol::Feature::BeatConfidence);
    audio.beatStrength = snapshot.feature(AudioProtocol::Feature::BeatStrength);

    audio.stereoWidth = snapshot.feature(AudioProtocol::Feature::StereoWidth);
    audio.stereoBalance = snapshot.feature(AudioProtocol::Feature::StereoBalance);
    audio.stereoCorrelation = snapshot.feature(AudioProtocol::Feature::StereoCorrelation);
    audio.energyTrend = snapshot.feature(AudioProtocol::Feature::EnergyTrend);

    audio.drop = snapshot.feature(AudioProtocol::Feature::Drop);
    audio.sectionChange = snapshot.feature(AudioProtocol::Feature::SectionChange);
    audio.spectralCentroid = snapshot.feature(AudioProtocol::Feature::SpectralCentroid);
    audio.spectralFlux = snapshot.feature(AudioProtocol::Feature::SpectralFlux);
    return audio;
}
}
void ShadertoyDesktop::after_swap(ImGuiContext *imCtx)
{
    if (currShaderHasError.load(std::memory_order_relaxed) || !isActive.load(std::memory_order_relaxed))
        return;

    if (hasUrlChanged)
    {
        if (!custom_shader_code.empty())
        {
            spdlog::info("Custom shader changed, loading from code...");
            loadLocalShaderFromCode(custom_shader_name, custom_shader_code);
        }
        else
        {
            spdlog::info("URL changed, loading shader {}...", url);
            loadCacheFromUrl(url);
        }
    }

    ImGui::SetCurrentContext(imCtx);
    runtime.setAudioInput(currentAudioInput());
    runtime.tick(60);

    const std::vector<uint8_t> data =
        runtime.renderToBuffer(ShaderToy::Vec2{static_cast<float>(width), static_cast<float>(height)});

    std::unique_lock lock(currDataMutex);
    currData = data;
}

void ShadertoyDesktop::renderCanvasCallback(const ImDrawList*, const ImDrawCmd* command)
{
    auto* self = static_cast<ShadertoyDesktop*>(command->UserCallbackData);
    const auto* draw_data = ImGui::GetDrawData();
    if (self == nullptr || draw_data == nullptr || !self->runtime.isValid())
        return;

    const ImVec2 framebuffer_size{
        draw_data->DisplaySize.x * draw_data->FramebufferScale.x,
        draw_data->DisplaySize.y * draw_data->FramebufferScale.y
    };
    const ImVec2 clip_offset = draw_data->DisplayPos;
    const ImVec2 clip_scale = draw_data->FramebufferScale;
    const ImVec2 clip_min{
        (command->ClipRect.x - clip_offset.x) * clip_scale.x,
        (command->ClipRect.y - clip_offset.y) * clip_scale.y
    };
    const ImVec2 clip_max{
        (command->ClipRect.z - clip_offset.x) * clip_scale.x,
        (command->ClipRect.w - clip_offset.y) * clip_scale.y
    };
    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
        return;

    self->runtime.render(ShaderToy::RenderRegion{
        .framebufferSize = {framebuffer_size.x, framebuffer_size.y},
        .clipMin = {clip_min.x, clip_min.y},
        .clipMax = {clip_max.x, clip_max.y},
        .canvasSize = {static_cast<float>(self->width), static_cast<float>(self->height)}
    });
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

    if (!custom_shader_code.empty())
    {
        ImGui::Text("Current Custom Shader: %s", custom_shader_name.c_str());
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

            // Match shadertoy v2's renderer integration: schedule the OpenGL
            // draw inside ImGui's draw list so its clip rectangle/framebuffer
            // coordinates are valid when the command is executed.
            auto* draw_list = ImGui::GetWindowDrawList();
            if (runtime.isValid())
            {
                draw_list->AddCallback(renderCanvasCallback, this);
                draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            }
            ImGui::Dummy(size);
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
        auto res = runtime.loadFromShaderToyResponse(shaderIdFromUrl(url), cached.value());
        if (!res.has_value())
        {
            spdlog::error("Failed to load from cache: {}", res.error().what());
            send_websocket_message("next_shader");
            currShaderHasError.store(true, std::memory_order_relaxed);
            return;
        }
        hasUrlChanged = false;
    }
    else
    {
        spdlog::info("Loading shader {} from ShaderToy...", url);
        auto res = runtime.loadFromShaderToy(url);
        if (!res.has_value())
        {
            spdlog::error("Failed to load from shadertoy: {}", res.error().what());
            send_websocket_message("next_shader");
            currShaderHasError.store(true, std::memory_order_relaxed);
            return;
        }
        hasUrlChanged = false;
    }
}

void ShadertoyDesktop::loadLocalShaderFromCode(const std::string &name, const std::string &code)
{
    auto res = runtime.loadImageShader(name, code, 0);
    if (!res.has_value())
    {
        spdlog::error("Failed to prepare custom shader '{}': {}", name, res.error().what());
        currShaderHasError.store(true, std::memory_order_relaxed);
        return;
    }

    hasUrlChanged = false;
    currShaderHasError.store(false, std::memory_order_relaxed);
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

    if (message == "shadertoy_inactive")
    {
        shaderRequested.store(false, std::memory_order_relaxed);
        return;
    }

    if (message.starts_with("url:"))
    {
        std::string newUrl = message.substr(4);
        bool urlChanged = newUrl != url || currShaderHasError.load(std::memory_order_relaxed);

        custom_shader_code.clear();
        custom_shader_name.clear();
        shaderRequested.store(true, std::memory_order_relaxed);
        url = newUrl;
        hasUrlChanged = urlChanged;
        currShaderHasError.store(false, std::memory_order_relaxed);
    }
    else if (message.starts_with("custom_shader:"))
    {
        try {
            auto payload = nlohmann::json::parse(message.substr(14));
            std::string name = payload.value("name", "unknown");
            std::string code = payload.value("code", "");
            
            bool changed = (code != custom_shader_code) || (name != custom_shader_name) ||
                           currShaderHasError.load(std::memory_order_relaxed);
            custom_shader_code = code;
            custom_shader_name = name;
            shaderRequested.store(true, std::memory_order_relaxed);
            url = "local://" + name;
            hasUrlChanged = changed;
            currShaderHasError.store(false, std::memory_order_relaxed);
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse custom_shader payload: {}", e.what());
        }
    }
}

std::optional<std::unique_ptr<UdpPacket>> ShadertoyDesktop::compute_next_packet(
    const std::string sceneName)
{
    const bool directShaderScene = sceneName == "shadertoy" || sceneName.starts_with("custom_shader:") ||
                                   sceneName.starts_with("shader:");
    const bool nestedShaderScene = sceneName == "music_director" && shaderRequested.load(std::memory_order_relaxed);
    if ((!directShaderScene && !nestedShaderScene) || width == 0 || height == 0 || !initError.empty())
    {
        isActive.store(false, std::memory_order_relaxed);
        return std::nullopt; // Not for this scene
    }

    isActive.store(true, std::memory_order_relaxed);
    std::shared_lock lock(currDataMutex);
    return std::make_unique<CanvasPacket>(currData);
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
