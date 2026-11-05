#pragma once

// Music style ("what kind of music is this") for scene casting.
//
// Two sources are fused:
//   * genre metadata for the current track (RuntimeInputIds::MusicGenre,
//     published by the SpotifyScenes genre lookup), mapped onto a few broad
//     visual styles, and
//   * a light-weight audio estimator fed by every analysis frame, which works
//     without Spotify and breaks ties for tracks without metadata.
//
// Scenes opt into styles with descriptor tags of the form "style:<name>",
// e.g. "style:chill". Both directors use style_fit() to rank candidates.

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <shared/common/audio_protocol.h>
#include <shared/matrix/runtime_inputs.h>

namespace MusicStyle {

enum class Style : int { Chill = 0, Edm, Party, Rock, HipHop, Acoustic, Count };
inline constexpr int StyleCount = static_cast<int>(Style::Count);
using Weights = std::array<float, StyleCount>;

[[nodiscard]] std::string_view name(Style style);
[[nodiscard]] std::string tag(Style style);  // "style:<name>"

/// Map free-form genre names ("Electro", "heavy metal", "Rap/Hip Hop",
/// "Klassik") onto normalised style weights. All zero when nothing matched.
[[nodiscard]] Weights from_genres(const std::vector<std::string> &genres);
[[nodiscard]] std::vector<std::string> split_genres(std::string_view comma_separated);

/// Rolling audio-character estimate (~20 s memory). Cheap enough to update for
/// every analysis packet.
class AudioEstimator {
public:
    void update(const AudioProtocol::Frame &frame, float dt);
    void reset();
    [[nodiscard]] Weights weights() const;
    /// 0..1: how much listening history backs the estimate.
    [[nodiscard]] float confidence() const;

private:
    float seconds_ = 0.0f;
    float loud_ = 0.3f, dynamics_ = 0.1f, onsetRate_ = 1.0f, kick_ = 0.2f, snare_ = 0.1f, hihat_ = 0.1f;
    float centroid_ = 0.4f, flatness_ = 0.2f, steady_ = 0.3f, bpm_ = 110.0f, lowShare_ = 0.4f;
    uint64_t lastOnset_ = 0;
    bool primed_ = false;
};

struct Info {
    Weights weights{};
    Style dominant = Style::Chill;
    float confidence = 0.0f;  // 0 = unknown
    std::string source;       // "genre:deezer", "audio", ...
    std::string genres;       // raw metadata genres when known
};

/// Fuse metadata + audio estimate from the current Runtime Inputs.
[[nodiscard]] Info describe(const RuntimeInputs::Snapshot &snapshot);

/// 0..1: share of the current music style a scene with these tags supports.
/// Scenes without any "style:" tag get a neutral `untagged` value.
[[nodiscard]] float style_fit(const Info &info, const std::vector<std::string> &tags, float untagged = 0.35f);

/// Runtime-input field names the audio estimator is published under (inside
/// the coarse RuntimeInputIds::Audio input).
[[nodiscard]] std::string audio_field(Style style);  // "style_<name>"
inline constexpr std::string_view AudioConfidenceField = "style_confidence";

}  // namespace MusicStyle
