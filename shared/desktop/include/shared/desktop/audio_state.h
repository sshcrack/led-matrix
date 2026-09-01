#pragma once

#include <chrono>

#include <shared/common/audio_protocol.h>
#include <shared/desktop/macro.h>

namespace DesktopAudioState {

/// Process-local high-frequency music analysis shared by desktop plugins.
///
/// This intentionally avoids the websocket/UDP path: AudioVisualizer already
/// performs the expensive capture/analysis work, while renderers such as
/// Shadertoy can consume the latest immutable snapshot at frame rate.
struct Snapshot : AudioProtocol::Frame {
    bool available = false;
    float age_seconds = 1000.0f;
    float sample_rate = 0.0f;

    [[nodiscard]] bool fresh(float max_age_seconds = 0.75f) const {
        return available && age_seconds <= max_age_seconds;
    }
};

SHARED_DESKTOP_API void update(const AudioProtocol::Frame &frame, float sample_rate = 0.0f);
SHARED_DESKTOP_API void clear();
SHARED_DESKTOP_API Snapshot snapshot();

} // namespace DesktopAudioState
