#pragma once

#include <shared/common/timesource/TimeSource.h>
#include <shared/matrix/Scene.h>
#include <shared/matrix/transition_composer.h>
#include <shared/matrix/transition_manager.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

#include "emulator.h"
#include "led-matrix.h"

/// The desktop scene worker's render state, kept out of main.cpp so tests can drive it
/// in-process: one remote scene session (`start`/`apply_state`/`reset`), the frames it
/// sent (so a transition can start from one of them), and the transition into it.
namespace SceneWorker {

/// Copies `canvas` into a packed RGB buffer (width * height * 3).
std::vector<std::uint8_t> capture(rgb_matrix::FrameCanvas* canvas, int width, int height);

/// Applies the audio frame the matrix relays with `state` commands (or clears it).
void apply_audio(const nlohmann::json& item);

/// A clock a test steps by hand.
class ManualClock final : public TimeSource {
public:
    int64_t now_ms() override { return now_; }
    void advance_ms(int64_t ms) { now_ += ms; }

private:
    int64_t now_ = 0;
};

struct RenderRuntime {
    using Clock = std::chrono::steady_clock;

    /// `transition_manager` is the daemon-independent transition catalogue owned by the worker's
    /// main(). `clock` times the blend (default: the wall clock).
    explicit RenderRuntime(TransitionManager& transition_manager, TimeSource* clock = nullptr);
    ~RenderRuntime() { reset(); }
    RenderRuntime(const RenderRuntime&) = delete;
    RenderRuntime& operator=(const RenderRuntime&) = delete;

    TransitionManager& transitions;
    TimeSource* clock;
    WallClock wall_clock;

    rgb_matrix::EmulatorMatrix* matrix = nullptr;
    rgb_matrix::FrameCanvas* canvas = nullptr;
    std::unique_ptr<Scenes::Scene> scene;
    std::uint32_t session = 0;
    std::uint32_t sequence = 0;
    int width = 0;
    int height = 0;
    int target_fps = 60;
    Clock::time_point next_frame{};

    /// Composes the blend into the session's first frames. The worker still owns the outgoing
    /// scene after a desktop -> desktop handoff, so it keeps it moving (`KeepMoving`); the Pi
    /// freezes its outgoing scene.
    TransitionComposer composer;
    rgb_matrix::FrameCanvas* transition_from = nullptr;
    rgb_matrix::FrameCanvas* transition_output = nullptr;

    /// Recently sent frames, so a transition can start from the frame the Pi
    /// shows (`from_frame`) without the Pi sending it back. ~16 frames cover
    /// the Pi's display lag; frames are 49 KB at 128x128.
    struct SentFrame {
        std::uint32_t session = 0;
        std::uint32_t sequence = 0;
        std::vector<std::uint8_t> rgb;
    };
    static constexpr std::size_t SentFrameHistory = 16;
    std::deque<SentFrame> sent_frames;

    void remember_sent_frame(const std::vector<std::uint8_t>& rgb);

    /// The referenced frame, else the closest one of that session.
    [[nodiscard]] const std::vector<std::uint8_t>* find_sent_frame(const nlohmann::json& reference) const;

    /// Desktop -> desktop transitions: the outgoing scene keeps animating under
    /// the blend instead of standing still on its last frame. It lives in its own
    /// matrix (which owns its canvas) until the blend ends.
    std::unique_ptr<Scenes::Scene> outgoing_scene;
    rgb_matrix::EmulatorMatrix* outgoing_matrix = nullptr;
    rgb_matrix::FrameCanvas* outgoing_canvas = nullptr;

    void drop_outgoing();

    /// Advance the outgoing scene one frame into the blend. Once the blend is over (or the
    /// scene stops or throws) it is released; the blend then continues from its last frame.
    void render_outgoing();

    /// The frame to send for the scene's latest render: the blend while one runs, else the scene's canvas.
    rgb_matrix::FrameCanvas* output_frame();

    void reset();
    void apply_state(const nlohmann::json& command);
    void start(const nlohmann::json& command);
};

}  // namespace SceneWorker
