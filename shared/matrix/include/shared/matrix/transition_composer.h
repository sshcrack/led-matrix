#pragma once

#include <string>

#include "led-matrix.h"
#include "shared/common/utils/utils.h"
#include "shared/matrix/transition_manager.h"

/// The rules for composing one scene-to-scene transition, shared by the Pi
/// (`TransitionEngine`) and the desktop scene worker (`RenderRuntime`) so the two
/// cannot drift apart:
///
///  * **Name resolution.** The named effect, else "blend", else none (then the
///    incoming frame is shown as is).
///  * **Easing.** Progress is smoothstepped. Linear alpha makes the first frames of
///    high-contrast scenes read as a luminance pop on LEDs.
///  * **Missing incoming frame.** Until the incoming scene has produced a real
///    frame (paused media, async artwork, ...) its canvas is replaced by the
///    outgoing snapshot, so the blend is visually neutral instead of exposing
///    whatever pixels were left in a reusable buffer.
///  * **Outgoing policy** (see `OutgoingPolicy`): whether the outgoing image may
///    keep changing during the blend. This is the one deliberate difference
///    between the hosts.
///
/// The composer owns no canvases and renders no scenes; it only decides what the
/// blend looks like at a given time. Time is the caller's clock in milliseconds
/// (the Pi's `TimeSource`), so tests can step it. Not thread-safe.
class TransitionComposer {
public:
    /// Whether the outgoing image may change while the blend runs.
    enum class OutgoingPolicy {
        /// The outgoing image is the snapshot passed to `begin()` for the whole
        /// blend. The Pi uses this: rendering both scenes during a transition would
        /// double the worst-case workload of a Raspberry Pi 3 for almost no
        /// perceptual benefit once the old scene is fading away.
        Freeze,
        /// The host may keep the outgoing scene moving by passing each new frame to
        /// `refresh_outgoing()`. The desktop scene worker uses this when it still
        /// owns the outgoing scene (desktop -> desktop); it has the CPU/GPU for it.
        KeepMoving,
    };

    /// What `frame()` produced.
    struct Frame {
        /// Eased progress in [0, 1] the frame was composed at.
        float eased = 0.0f;
        /// The blend is over: the incoming scene should be shown from now on.
        bool done = false;
    };

    explicit TransitionComposer(const TransitionManager* effects, OutgoingPolicy policy = OutgoingPolicy::Freeze)
        : effects_(effects), policy_(policy)
    {
    }

    /// Smoothstep used for every transition.
    [[nodiscard]] static float ease(float linear_progress);

    /// Starts a blend that began at `start_ms`. `from` holds the outgoing snapshot
    /// and must stay valid until the blend has ended. `name` falls back to
    /// "blend". Returns false when no effect resolved; `frame()` then shows the
    /// incoming scene without blending.
    bool begin(FrameCanvas* from, const std::string& name, tmillis_t duration_ms, int width, int height, tmillis_t start_ms);

    /// Drops the current blend.
    void end();

    /// True from `begin()` until the blend is over and the incoming scene has
    /// produced a real frame.
    [[nodiscard]] bool active() const { return from_ != nullptr && !(finished_ && have_incoming_); }

    /// True from `begin()` until the blend itself is over (progress reached 1).
    [[nodiscard]] bool blending() const { return from_ != nullptr && !finished_; }

    [[nodiscard]] OutgoingPolicy outgoing_policy() const { return policy_; }

    /// Feeds a newer outgoing frame. Only honoured with `KeepMoving`; a `Freeze`
    /// composer keeps its snapshot. Returns whether the snapshot changed.
    bool refresh_outgoing(const FrameCanvas& latest);

    /// Eased progress at `now_ms` without composing anything.
    [[nodiscard]] float eased_progress(tmillis_t now_ms) const;

    /// Composes the blend at `now_ms` into `out`. `to` is the incoming scene's
    /// canvas and `to_has_new_pixels` says whether it received a real frame since
    /// the last call. Call it only while `active()`.
    Frame frame(FrameCanvas* out, FrameCanvas* to, bool to_has_new_pixels, tmillis_t now_ms);

private:
    [[nodiscard]] float linear_progress(tmillis_t now_ms) const;

    const TransitionManager* effects_;
    OutgoingPolicy policy_;
    FrameCanvas* from_ = nullptr;
    TransitionEffect* effect_ = nullptr;
    tmillis_t start_ms_ = 0;
    tmillis_t duration_ms_ = 1;
    int width_ = 0;
    int height_ = 0;
    bool have_incoming_ = false;
    bool finished_ = false;
};
