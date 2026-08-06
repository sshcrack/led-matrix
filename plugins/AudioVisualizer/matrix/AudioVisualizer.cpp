#include "AudioVisualizer.h"
#include "scenes/AudioReactiveScenes.h"
#include "scenes/AudioSpectrumScene.h"

#include <shared/common/audio_protocol.h>
#include <shared/matrix/canvas_consts.h>
#include <spdlog/spdlog.h>

using namespace Scenes;

REGISTER_PLUGIN(AudioVisualizer, AudioVisualizer)

std::vector<std::unique_ptr<ImageProviderWrapper>> AudioVisualizer::create_image_providers() {
    return {};
}

std::vector<std::unique_ptr<SceneWrapper>> AudioVisualizer::create_scenes() {
    std::vector<std::unique_ptr<SceneWrapper>> scenes;
    scenes.push_back(std::make_unique<AudioSpectrumSceneWrapper>());
    scenes.push_back(std::make_unique<AudioParticleFieldSceneWrapper>());
    scenes.push_back(std::make_unique<AudioPulseTunnelSceneWrapper>());
    scenes.push_back(std::make_unique<AudioAuroraSceneWrapper>());
    scenes.push_back(std::make_unique<AudioKaleidoscopeSceneWrapper>());
    return scenes;
}

std::optional<std::string> AudioVisualizer::before_server_init() {
    spdlog::debug("Starting rich music-analysis UDP receiver");
    return std::nullopt;
}

std::optional<std::string> AudioVisualizer::pre_exit() {
    spdlog::debug("Stopping rich music-analysis UDP receiver");
    return std::nullopt;
}

AudioState::Snapshot AudioVisualizer::get_audio_state() const {
    return AudioState::snapshot();
}

bool AudioVisualizer::on_udp_packet(uint8_t pluginId, const uint8_t *data, size_t size) {
    if (pluginId != 0x01) return false;

    AudioProtocol::Frame frame;
    std::string error;
    if (!AudioProtocol::decode(std::span<const uint8_t>(data, size), frame, &error)) {
        spdlog::warn("Rejected music-analysis packet: {}", error);
        return false;
    }

    AudioState::update(frame);

    if (frame.event(AudioProtocol::DropEvent) && Constants::global_post_processor)
        Constants::global_post_processor->add_effect("flash", 0.28f, 0.55f);
    return true;
}
