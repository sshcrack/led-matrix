#pragma once

#include <chrono>

#include <string>
#include <string_view>
#include <vector>

#include "shared/common/audio_protocol.h"

#include "shared/matrix/preview.h"

class AudioPreviewProvider final : public Previews::DataProvider {
public:
    [[nodiscard]] std::string_view id() const override { return Previews::Inputs::Audio; }
    void begin(const Previews::RunContext &context) override;
    void update(const Scenes::SceneFrameContext &frame) override;
    void end() noexcept override;

private:
    int fps_ = 15;
    float bpm_ = 120.0f;
    std::string profile_ = "balanced";
    // Optional recorded analysis (tests/audio_analysis_dump) replayed instead
    // of the synthetic fixture: --preview-option audio:replay="file.audr".
    std::vector<AudioProtocol::Frame> replay_;
    int replay_fps_ = 30;
    std::chrono::steady_clock::time_point replay_start_{};
};
