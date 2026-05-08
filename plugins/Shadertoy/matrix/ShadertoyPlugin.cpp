#include "ShadertoyPlugin.h"

#include <shared/matrix/plugin_loader/loader.h>

#include "scenes/ShadertoyScene.h"
#include "providers/Random.h"
#include "providers/Collection.h"
#include "shared/matrix/canvas_consts.h"
#include "spdlog/spdlog.h"
#include "shared/matrix/utils/shared.h"
#include <filesystem>
#include <chrono>

using namespace Scenes;
using namespace ShaderProviders;
namespace fs = std::filesystem;

namespace {
fs::path custom_shader_dir() {
    return get_exec_dir() / "data" / "custom_shaders";
}
}

extern "C" PLUGIN_EXPORT ShadertoyPlugin *createShadertoy()
{
    return new ShadertoyPlugin();
}

extern "C" PLUGIN_EXPORT void destroyShadertoy(ShadertoyPlugin *c)
{
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>>
ShadertoyPlugin::create_image_providers()
{
    return {};
}

vector<std::unique_ptr<ShaderProviderWrapper, void (*)(ShaderProviderWrapper *)>>
ShadertoyPlugin::create_shader_providers()
{
    auto providers = vector<std::unique_ptr<ShaderProviderWrapper, void (*)(ShaderProviderWrapper *)>>();
    auto deleteWrapper = [](ShaderProviderWrapper *wrapper) {
        delete wrapper;
    };

    providers.push_back({new RandomWrapper(), deleteWrapper});
    providers.push_back({new CollectionWrapper(), deleteWrapper});

    return providers;
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> ShadertoyPlugin::create_scenes()
{
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>>();
    auto deleteScene = [](SceneWrapper *scene)
    {
        delete scene;
    };

    scenes.push_back({new ShadertoySceneWrapper(), deleteScene});

    const auto custom_dir = custom_shader_dir();
    std::error_code ec;
    fs::create_directories(custom_dir, ec);
    if (ec) {
        spdlog::warn("Shadertoy: failed to create custom shader dir '{}': {}", custom_dir.string(), ec.message());
        return scenes;
    }

    for (const auto &entry : fs::directory_iterator(custom_dir, ec)) {
        if (ec) {
            spdlog::warn("Shadertoy: failed to scan custom shader dir '{}': {}", custom_dir.string(), ec.message());
            break;
        }

        if (!entry.is_regular_file() || entry.path().extension() != ".frag") {
            continue;
        }

        auto *custom_wrapper = new CustomShadertoySceneWrapper(entry.path());
        customSceneNamesByFile[entry.path().string()] = custom_wrapper->get_name();
        scenes.push_back({custom_wrapper, deleteScene});
    }

    return scenes;
}

bool ShadertoyPlugin::on_udp_packet(const uint8_t pluginId, const uint8_t *packetData,
                                    const size_t size)
{
    if (pluginId != 0x02)
        return false; // Not destined for this plugin

    int neededPacketSize = Constants::height * Constants::width * 3; // 3 bytes per pixel (RGB)
    static int consecutiveErrors = 0;
    if (size < neededPacketSize)
    {
        consecutiveErrors++;
        if (consecutiveErrors < 10)
            spdlog::error("Received packet too small: {} bytes, expected at least {} bytes", size, neededPacketSize);
        return false; // Invalid packet size
    }

    consecutiveErrors = 0;
    std::unique_lock lock(dataMutex);
    data.assign(packetData, packetData + neededPacketSize);

    return true;
}

std::optional<std::vector<std::string>> ShadertoyPlugin::on_websocket_open()
{
    std::shared_lock lock(Server::currSceneMutex);
    std::string sizeMsg = "size:" + std::to_string(Constants::width) + "x" + std::to_string(Constants::height);
    std::vector<std::string> v = {sizeMsg};
    
    std::lock_guard<std::mutex> lock2(lastMsgMutex);
    if (!last_sent_message.empty()) {
        v.push_back(last_sent_message);
    }
    
    return v;
}

void ShadertoyPlugin::on_websocket_message(const std::string &message)
{
    if (message == "next_shader")
        Scenes::switchToNextRandomShader = true;
}

std::string ShadertoyPlugin::add_custom_shader_scene(const std::filesystem::path &shader_file_path) {
    std::lock_guard lock(customSceneMutex);
    const auto file_key = shader_file_path.string();
    if (customSceneNamesByFile.contains(file_key)) {
        return customSceneNamesByFile[file_key];
    }

    auto deleter = [](SceneWrapper *scene) { delete scene; };
    std::shared_ptr<Plugins::SceneWrapper> wrapper(new CustomShadertoySceneWrapper(shader_file_path), deleter);
    const auto scene_name = wrapper->get_name();
    customSceneNamesByFile[file_key] = scene_name;
    Plugins::PluginManager::instance()->add_scene(std::move(wrapper));
    spdlog::info("Shadertoy: registered custom shader scene '{}' from '{}'", scene_name, file_key);
    return scene_name;
}

std::string ShadertoyPlugin::remove_custom_shader_scene(const std::filesystem::path &shader_file_path) {
    std::lock_guard lock(customSceneMutex);
    const auto file_key = shader_file_path.string();
    auto it = customSceneNamesByFile.find(file_key);
    const auto scene_name = it != customSceneNamesByFile.end()
                                ? it->second
                                : ("custom_shader:" + shader_file_path.stem().string());
    Plugins::PluginManager::instance()->remove_scene(scene_name);
    if (it != customSceneNamesByFile.end()) {
        customSceneNamesByFile.erase(it);
    }
    spdlog::info("Shadertoy: removed custom shader scene '{}' from '{}'", scene_name, file_key);
    return scene_name;
}

std::optional<std::string> ShadertoyPlugin::after_server_init() {
    stop_watcher_ = false;
    watcher_thread_ = std::thread(&ShadertoyPlugin::watch_custom_shader_dir, this);
    return std::nullopt;
}

std::optional<std::string> ShadertoyPlugin::pre_exit() {
    stop_watcher_ = true;
    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
    return std::nullopt;
}

void ShadertoyPlugin::watch_custom_shader_dir() {
    using namespace std::chrono_literals;
    const auto custom_dir = custom_shader_dir();

    while (!stop_watcher_) {
        std::this_thread::sleep_for(1s);
        if (stop_watcher_) {
            break;
        }

        std::error_code ec;
        fs::create_directories(custom_dir, ec);
        if (ec) {
            continue;
        }

        std::unordered_map<std::string, bool> found_files;
        for (const auto &entry : fs::directory_iterator(custom_dir, ec)) {
            if (ec) {
                break;
            }
            if (!entry.is_regular_file() || entry.path().extension() != ".frag") {
                continue;
            }
            const auto key = entry.path().string();
            found_files[key] = true;

            add_custom_shader_scene(entry.path());
        }

        std::vector<std::string> to_remove;
        {
            std::lock_guard lock(customSceneMutex);
            for (const auto &[file_key, scene_name] : customSceneNamesByFile) {
                if (!found_files.contains(file_key)) {
                    to_remove.push_back(file_key);
                }
            }
        }

        for (const auto &file_key : to_remove) {
            remove_custom_shader_scene(file_key);
        }
    }
}
