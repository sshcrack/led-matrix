#pragma once
#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

// The browser-driven Spotify sign-in, as one value instead of loose pending_* strings.
//
//   Idle ──begin()──▶ Waiting ──take(matching state)──▶ Exchanging ──finish()──▶ Connected | Failed
//                       │  └─ 10 min pass ─▶ Expired (derived; take() then records it)
//                       └─ begin() again / cancel() ─▶ replaced (the old attempt can no longer finish)
//
// Every begin() gets a new attempt id. /spotify/status reports the latest attempt's id and phase, so the
// browser that started an attempt learns how *that* attempt ended, even when Spotify was already
// connected (a reconnect) or when sign-in completed in another tab or on another device.
// Header-only so tests/spotify_login_attempt_smoke.cpp can drive it without the plugin.
namespace SpotifyLogin {
    using Clock = std::chrono::steady_clock;
    inline constexpr auto kLifetime = std::chrono::minutes(10);

    enum class Phase { Idle, Waiting, Exchanging, Connected, Failed, Expired };

    inline std::string_view to_string(Phase p) {
        switch (p) {
            case Phase::Idle: return "idle";
            case Phase::Waiting: return "waiting";
            case Phase::Exchanging: return "exchanging";
            case Phase::Connected: return "connected";
            case Phase::Failed: return "failed";
            case Phase::Expired: return "expired";
        }
        return "idle";
    }

    /// Secrets of one started sign-in; never leave the Pi.
    struct Pending {
        std::string state;    // OAuth state echoed back by Spotify (one-time)
        std::string verifier; // PKCE code_verifier
        std::string redirect; // redirect_uri the authorize URL used (must match at token exchange)
    };

    struct Snapshot {
        std::uint64_t attempt = 0; // 0 = no attempt since the daemon started
        Phase phase = Phase::Idle;
        std::string error;         // set for Failed and Expired
    };

    inline constexpr std::string_view kExpiredMessage =
        "This Spotify login has expired or does not match. Choose Connect Spotify and try again.";

    class Attempt {
    public:
        /// Starts a new attempt and supersedes any earlier one. Returns its id.
        std::uint64_t begin(Pending pending, Clock::time_point now) {
            pending_ = std::move(pending);
            expires_ = now + kLifetime;
            snapshot_ = {++last_id_, Phase::Waiting, {}};
            return last_id_;
        }

        /// Accepts the state Spotify sent back and hands over the secrets for the token exchange.
        /// The pending login is consumed whether or not the exchange later succeeds (one-time use).
        /// A state from an older or foreign attempt is rejected without touching the current one.
        std::expected<Pending, std::string> take(const std::string& state, Clock::time_point now) {
            if (!pending_ || state.empty() || state != pending_->state)
                return std::unexpected(std::string(kExpiredMessage));
            if (now > expires_) {
                pending_.reset();
                snapshot_ = {snapshot_.attempt, Phase::Expired, std::string(kExpiredMessage)};
                return std::unexpected(std::string(kExpiredMessage));
            }
            Pending taken = std::move(*pending_);
            pending_.reset();
            snapshot_.phase = Phase::Exchanging;
            return taken;
        }

        /// Records how the exchange of the attempt taken last ended.
        void succeed() { if (snapshot_.phase == Phase::Exchanging) snapshot_ = {snapshot_.attempt, Phase::Connected, {}}; }
        void fail(std::string message) {
            if (snapshot_.phase == Phase::Exchanging) snapshot_ = {snapshot_.attempt, Phase::Failed, std::move(message)};
        }

        /// Credentials changed: the started attempt can no longer finish.
        void cancel() {
            if (!pending_ && snapshot_.phase != Phase::Exchanging) return;
            pending_.reset();
            snapshot_ = {snapshot_.attempt, Phase::Failed, "The Spotify Client ID changed. Choose Connect Spotify again."};
        }

        [[nodiscard]] Snapshot snapshot(Clock::time_point now) const {
            if (snapshot_.phase == Phase::Waiting && now > expires_)
                return {snapshot_.attempt, Phase::Expired, std::string(kExpiredMessage)};
            return snapshot_;
        }

    private:
        std::optional<Pending> pending_;
        Clock::time_point expires_{};
        std::uint64_t last_id_ = 0;
        Snapshot snapshot_;
    };
} // namespace SpotifyLogin
