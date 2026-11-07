#pragma once

#include <cstdint>
#include <optional>

/// Where the frames of one scene-presentation phase come from.
///
///   Local ──request_desktop──▶ DesktopPending ──remote frame──▶ DesktopLive
///     ▲                          (local keeps rendering)          │    ▲
///     │                                                 stall >   │    │ remote frame
///     │                                                 hold_ms   ▼    │
///     └───────── worker lost (from any desktop state) ──── LocalFallback
///
/// In DesktopLive a stall up to `RemoteStallHoldMs` holds the last desktop
/// frame instead of rendering locally: the local simulation has diverged from
/// the desktop's, so alternating the two would read as flicker. Only a longer
/// outage drops to LocalFallback, which renders locally but keeps the session
/// so fresh desktop frames resume DesktopLive.
///
/// The type is pure (no clock, no RemoteRender calls) so the transitions are
/// unit-testable; DesktopHandoff feeds it observations each frame.
class RenderPlacement {
public:
    enum class Mode { Local, DesktopPending, DesktopLive, LocalFallback };
    /// What the renderer should do for the current frame when no new remote
    /// frame was copied.
    enum class NoRemoteFrame { HoldRemote, RenderLocal };

    static constexpr double RemoteStallHoldMs = 1500.0;
    static constexpr double PressureBudgetFraction = 0.72;
    static constexpr unsigned PressureFramesToOffload = 4;

    [[nodiscard]] Mode mode() const { return mode_; }
    [[nodiscard]] std::optional<std::uint32_t> session() const { return session_; }
    [[nodiscard]] bool has_session() const { return session_.has_value(); }

    /// Local → DesktopPending. A nullopt session (request refused) stays Local.
    void request_desktop(std::optional<std::uint32_t> session)
    {
        if (!session.has_value()) return;
        session_ = session;
        mode_ = Mode::DesktopPending;
        pressure_streak_ = 0;
    }

    /// Any desktop state → Local. Returns true if a session was dropped.
    bool worker_lost()
    {
        if (!session_.has_value()) return false;
        session_.reset();
        mode_ = Mode::Local;
        pressure_streak_ = 0;
        return true;
    }

    /// A complete remote frame was presented. Returns true on the transition
    /// into DesktopLive (first frame, or recovery from LocalFallback).
    bool remote_frame_presented()
    {
        if (!session_.has_value() || mode_ == Mode::DesktopLive) return false;
        mode_ = Mode::DesktopLive;
        return true;
    }

    /// No new remote frame this tick. `latest_age_ms` is the age of the last
    /// remote frame of this session, if any. DesktopLive either holds or
    /// falls back to LocalFallback; other modes render locally.
    NoRemoteFrame no_remote_frame(std::optional<double> latest_age_ms)
    {
        if (mode_ != Mode::DesktopLive) return NoRemoteFrame::RenderLocal;
        if (latest_age_ms.has_value() && *latest_age_ms <= RemoteStallHoldMs)
            return NoRemoteFrame::HoldRemote;
        mode_ = Mode::LocalFallback;
        return NoRemoteFrame::RenderLocal;
    }

    /// Records a local frame's active render cost. Returns true when a Local
    /// placement has been under sustained pressure long enough to ask for the
    /// desktop; the streak restarts after each such request.
    bool local_frame_rendered(double active_render_ms, double budget_ms)
    {
        if (active_render_ms > budget_ms * PressureBudgetFraction)
            ++pressure_streak_;
        else if (pressure_streak_ > 0)
            --pressure_streak_;
        if (mode_ != Mode::Local || pressure_streak_ < PressureFramesToOffload) return false;
        pressure_streak_ = 0;
        return true;
    }

    [[nodiscard]] static const char *label(Mode mode)
    {
        switch (mode) {
        case Mode::Local: return "local";
        case Mode::DesktopPending: return "desktop_pending";
        case Mode::DesktopLive: return "desktop";
        case Mode::LocalFallback: return "local_fallback";
        }
        return "local";
    }

private:
    Mode mode_ = Mode::Local;
    std::optional<std::uint32_t> session_;
    unsigned pressure_streak_ = 0;
};
