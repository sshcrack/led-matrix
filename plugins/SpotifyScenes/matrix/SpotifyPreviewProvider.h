#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "shared/matrix/preview.h"

class SpotifyPreviewProvider final : public Previews::DataProvider {
public:
    [[nodiscard]] std::string_view id() const override { return Previews::Inputs::SpotifyPlayback; }
    void begin(const Previews::RunContext &context) override;
    void update(const Scenes::SceneFrameContext &frame) override;
    void end() noexcept override;

private:
    static constexpr std::string_view TrackId = "preview-track";
    // One fixture track per cover. Options:
    //   spotify.playback:covers=["a.jpg","b.png"]  real images instead of the synthetic cover
    //   spotify.playback:track_seconds=6           switch to the next track every N seconds
    std::vector<std::filesystem::path> cover_paths_;
    std::vector<std::string> track_ids_;
    double track_seconds_ = 0.0;
    long duration_ms_ = 180000;

    [[nodiscard]] size_t track_index(double elapsed_seconds) const;
};
