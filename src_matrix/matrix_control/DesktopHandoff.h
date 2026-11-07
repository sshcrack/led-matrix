#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "RenderPlacement.h"
#include "shared/common/utils/utils.h"
#include "shared/matrix/remote_render.h"

namespace rgb_matrix { class FrameCanvas; }
namespace Scenes { class Scene; }

/// Port to the desktop scene worker. Production: `DesktopLink::remote_render()`
/// (the RemoteRender transport); tests supply an in-memory worker.
class DesktopLink {
public:
    virtual ~DesktopLink() = default;
    [[nodiscard]] virtual bool worker_available(std::string_view scene) = 0;
    virtual std::optional<std::uint32_t> request_scene(const Scenes::Scene &scene, int width, int height,
                                                       int target_fps, const nlohmann::json &transition) = 0;
    virtual void publish_runtime_state(std::uint32_t session) = 0;
    virtual bool copy_latest(std::uint32_t session, rgb_matrix::FrameCanvas *canvas, int width, int height,
                             double max_age_ms) = 0;
    [[nodiscard]] virtual std::optional<double> latest_frame_age_ms(std::uint32_t session) = 0;
    [[nodiscard]] virtual RemoteRender::Status status() = 0;
    virtual void stop() = 0;

    static DesktopLink &remote_render();
};

/// Desktop handoff: owns the desktop render session across the scene and
/// transition phases, so neither render loop interprets worker state itself.
///
///  - Scene phase: decides the placement up front (adopt the session a desktop
///    transition warmed for this scene, or offload a scene known to be too
///    expensive / quality-reduced on the Pi), then per frame says whether to
///    show a remote frame, hold the last one, or render locally (RenderPlacement:
///    a stall up to 1500 ms holds the desktop frame), and starts the desktop
///    under sustained local pressure.
///  - Transition phase: copies the outgoing scene's desktop frame, runs a
///    worker-composed transition (first frame within 500 ms, then no gap over
///    150 ms, or it is abandoned), or prepares a locally composed transition,
///    offloading the incoming scene by the same rule as a scene phase.
///
/// Rendering, transition composition and Pi presentation stay with the render
/// loops; this module only decides where frames come from.
class DesktopHandoff {
public:
    static constexpr tmillis_t TransitionStartupMs = 500;
    static constexpr tmillis_t TransitionStallMs = 150;
    static constexpr double TransitionFrameMaxAgeMs = 150.0;
    static constexpr double SceneFrameMaxAgeMs = 300.0;
    static constexpr float ReducedQualityThreshold = 0.97f;

    explicit DesktopHandoff(DesktopLink &link = DesktopLink::remote_render()) : link_(link) {}

    // ---- scene phase ----
    enum class FrameSource { Remote, HoldRemote, Local };

    /// Start the phase of `scene`: adopt, request or drop a desktop session and
    /// report the placement.
    void begin_scene(const Scenes::Scene &scene, int width, int height);
    /// Called once per frame before rendering. On `Remote` a desktop frame was
    /// copied into `canvas`; on `HoldRemote` the last presented frame stays.
    FrameSource scene_frame(rgb_matrix::FrameCanvas *canvas);
    /// The local active render cost of the frame just rendered.
    void local_frame_rendered(double active_render_ms);
    [[nodiscard]] RenderPlacement::Mode scene_mode() const { return placement_.mode(); }

    // ---- transition phase ----
    /// Copy the newest desktop frame of `scene` (the outgoing scene, whose local
    /// simulation may have stopped advancing) into `canvas`.
    bool copy_scene_frame(const Scenes::Scene &scene, rgb_matrix::FrameCanvas *canvas, int width, int height);

    /// Ask the worker to compose the transition into `next` from the outgoing
    /// pixels. False when the desktop cannot (then compose locally).
    bool begin_desktop_transition(const Scenes::Scene &next, const rgb_matrix::FrameCanvas &outgoing,
                                  int width, int height, const std::string &name, tmillis_t duration_ms,
                                  tmillis_t now_ms);
    enum class TransitionOutcome {
        Continue,   ///< keep ticking
        Done,       ///< the transition finished; the session continues with the scene
        Abandoned,  ///< the worker stalled after presenting: skip to the next scene
        Failed,     ///< the worker never produced a frame: compose locally instead
    };
    struct TransitionTick {
        bool frame = false;   ///< a new composed frame is in the canvas: present it (before acting on `outcome`)
        TransitionOutcome outcome = TransitionOutcome::Continue;
    };
    TransitionTick desktop_transition_tick(rgb_matrix::FrameCanvas *canvas, tmillis_t now_ms);
    struct TransitionStats { std::uint64_t frames = 0; tmillis_t startup_ms = 0; double presentation_fps = 0.0; };
    [[nodiscard]] TransitionStats desktop_transition_stats(tmillis_t now_ms) const;

    /// Before a locally composed transition into `next`: start the desktop for
    /// it when it is known to be too expensive locally, else stop any session.
    void begin_local_transition(const Scenes::Scene &next, int width, int height);
    /// Copy a desktop frame of the incoming scene, if it is desktop-rendered.
    bool incoming_frame(rgb_matrix::FrameCanvas *canvas);

private:
    DesktopLink &link_;
    /// The session the worker currently renders, and for which scene.
    std::optional<std::uint32_t> session_;
    std::string session_scene_;

    // Scene phase.
    const Scenes::Scene *scene_ = nullptr;
    int width_ = 0, height_ = 0;
    double budget_ms_ = 0.0;
    RenderPlacement placement_;
    std::string reason_;

    // Desktop transition.
    struct Transition {
        std::string next;
        tmillis_t duration_ms = 0;
        tmillis_t requested_at = 0, last_frame_at = 0, first_frame_at = 0;
        std::uint32_t last_sequence = 0;
        std::uint64_t frames = 0;
        int width = 0, height = 0;
    };
    std::optional<Transition> transition_;
    /// Locally composed transition: the incoming scene's desktop session, if any.
    std::optional<std::uint32_t> incoming_;
    int incoming_width_ = 0, incoming_height_ = 0;

    std::optional<std::uint32_t> request(const Scenes::Scene &scene, int width, int height, int fps,
                                         const nlohmann::json &transition = nlohmann::json::object());
    void stop();
    /// Why a scene should start on the desktop, if it should.
    [[nodiscard]] static std::optional<std::string> offload_reason(const Scenes::Scene &scene);
    void request_scene_desktop();
    void report(const std::string &reason, nlohmann::json extra = nlohmann::json::object());
};
