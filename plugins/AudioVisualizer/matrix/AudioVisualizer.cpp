#include "AudioVisualizer.h"

#include <shared/common/audio_protocol.h>
#include <shared/matrix/diagnostics.h>
#include <spdlog/spdlog.h>

#include "AudioPreviewProvider.h"
#include "scenes/AudioParticleScene.h"
#include "scenes/AudioSpectrumScene.h"
#include "scenes/MusicDirectorScene.h"

using namespace Scenes;

REGISTER_PLUGIN(AudioVisualizer, AudioVisualizer)

std::vector<std::unique_ptr<ImageProviderWrapper>> AudioVisualizer::create_image_providers()
{
    return {};
}

std::vector<std::unique_ptr<Previews::DataProvider>> AudioVisualizer::create_preview_data_providers()
{
    std::vector<std::unique_ptr<Previews::DataProvider>> providers;
    providers.push_back(std::make_unique<AudioPreviewProvider>());
    return providers;
}

std::vector<std::unique_ptr<SceneWrapper>> AudioVisualizer::create_scenes()
{
    std::vector<std::unique_ptr<SceneWrapper>> scenes;
    scenes.push_back(std::make_unique<AudioSpectrumSceneWrapper>());
    scenes.push_back(std::make_unique<AudioParticleFieldSceneWrapper>());
    scenes.push_back(std::make_unique<MusicDirectorSceneWrapper>());
    return scenes;
}

std::optional<std::string> AudioVisualizer::before_server_init()
{
    AudioState::clear();
    spdlog::debug("Starting rich music-analysis UDP receiver");
    return std::nullopt;
}

std::optional<std::string> AudioVisualizer::pre_exit()
{
    AudioState::clear();
    spdlog::debug("Stopping rich music-analysis UDP receiver");
    return std::nullopt;
}

AudioState::Snapshot AudioVisualizer::get_audio_state() const
{
    return AudioState::snapshot();
}

bool AudioVisualizer::on_udp_packet(uint8_t pluginId, const uint8_t* data, size_t size)
{
    if (pluginId != 0x01)
        return false;

    AudioProtocol::Frame frame;
    std::string error;
    if (!AudioProtocol::decode(std::span<const uint8_t>(data, size), frame, &error)) {
        Diagnostics::RuntimeDiagnostics::instance().record_audio_decode_error();
        spdlog::warn("Rejected music-analysis packet: {}", error);
        return false;
    }

    Diagnostics::RuntimeDiagnostics::instance().record_audio_packet(frame.sequence);
    AudioState::update(frame);

    return true;
}
