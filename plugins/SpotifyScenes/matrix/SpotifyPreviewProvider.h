#pragma once

#include <filesystem>
#include <string_view>

#include "shared/matrix/preview.h"

class SpotifyPreviewProvider final : public Previews::DataProvider {
public:
    [[nodiscard]] std::string_view id() const override { return Previews::Inputs::SpotifyPlayback; }
    void begin(const Previews::RunContext &context) override;
    void update(const Scenes::SceneFrameContext &frame) override;
    void end() noexcept override;

private:
    static constexpr std::string_view TrackId = "preview-track";
    std::filesystem::path cover_path_;
    long duration_ms_ = 180000;
};
