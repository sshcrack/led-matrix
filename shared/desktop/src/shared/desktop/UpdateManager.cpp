#pragma execution_character_set("utf-8")
#include "shared/desktop/UpdateManager.h"
#include "shared/desktop/SelfUpdate.h"
#include <shared/desktop/config.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace UpdateChecker
{
    /// Update workflow. Exactly one phase is current; each phase owns one dialog.
    ///
    ///   Idle ──check──▶ Checking ──auto / manual on a per-user install──▶ Downloading ──ok──▶ Ready
    ///                     │  ├──other install, update wanted──▶ Prompt ──update now──▶ Downloading
    ///                     │  └──manual: up to date / waiting for matrix / error──▶ Result
    ///                     └──automatic, nothing to do / failed──▶ Idle
    ///   Downloading ──fail──▶ Failed (shown) or Idle (silent automatic update)
    ///   Ready ──later──▶ ReadyDismissed;  Ready/ReadyDismissed ──restart──▶ app exits and restarts
    ///
    /// Checks run on a detached worker that shares `State` through a shared_ptr,
    /// so it may outlive the manager at shutdown.
    enum class Phase { Idle, Checking, Prompt, Result, Downloading, Failed, Ready, ReadyDismissed };

    namespace
    {
        using Clock = std::chrono::steady_clock;
        constexpr auto CHECK_INTERVAL = std::chrono::hours(1);
        // While the matrix installs a release we asked for, look again soon so
        // the desktop follows within a minute of the matrix coming back.
        constexpr auto MATRIX_WAIT_INTERVAL = std::chrono::minutes(1);
        constexpr auto MATRIX_WAIT_LIMIT = std::chrono::minutes(20);

        struct State
        {
            mutable std::mutex mutex;
            Phase phase = Phase::Idle;
            bool silent = false;       // Downloading: no progress dialog
            ReleaseInfo release;       // Prompt / Downloading / Ready
            std::string reason;        // Prompt: why this version
            std::string message;       // Result / Failed
            std::string status;        // menu status line
            Clock::time_point matrixWaitUntil{};
            std::atomic<size_t> downloaded{0};
            std::atomic<size_t> total{0};

            void set(Phase p)
            {
                std::lock_guard lock(mutex);
                phase = p;
            }
            Phase get() const
            {
                std::lock_guard lock(mutex);
                return phase;
            }
        };

        std::string v(const Common::Version &version) { return "v" + version.toString(); }

        void install(const std::shared_ptr<State> &state, const ReleaseInfo &release, bool silent)
        {
            {
                std::lock_guard lock(state->mutex);
                state->phase = Phase::Downloading;
                state->silent = silent;
                state->release = release;
                state->status = "Downloading " + v(release.version) + "…";
            }
            state->downloaded = 0;
            state->total = 0;
            auto result = SelfUpdate::download_and_install(
                release.downloadUrl, release.sha256, release.version.toString(),
                [state](size_t done, size_t total) {
                    state->downloaded = done;
                    state->total = total;
                });

            std::lock_guard lock(state->mutex);
            if (result)
            {
                spdlog::info("Desktop update {} installed; it starts on the next restart", v(release.version));
                state->phase = Phase::Ready;
                state->status = v(release.version) + " is installed; restart to use it";
            }
            else
            {
                spdlog::error("Desktop update to {} failed: {}", v(release.version), result.error());
                state->message = result.error();
                state->status = "Update to " + v(release.version) + " failed: " + result.error();
                state->phase = silent ? Phase::Idle : Phase::Failed;
            }
        }

        /// One check: decide the target version, nudge the matrix, install or prompt.
        void sync(const std::shared_ptr<State> &state, UpdatePreferences prefs, bool manual)
        {
            const Common::Version current = UpdateChecker::getCurrentVersion();
            auto finish = [&](const std::string &message, const std::string &status) {
                std::lock_guard lock(state->mutex);
                state->message = message;
                state->status = status;
                state->phase = manual ? Phase::Result : Phase::Idle;
            };

            const auto latest = UpdateChecker::fetchRelease();
            if (!latest)
                spdlog::warn("Desktop update check: {}", latest.error());

            std::optional<MatrixUpdateState> matrix;
            if (prefs.followMatrix)
            {
                const auto &general = Config::ConfigManager::instance()->getGeneralConfig();
                const auto host = general.getHostnameCopy();
                const int port = general.getPort();
                if (auto m = UpdateChecker::fetchMatrixState(host, port))
                    matrix = *m;
                else
                    spdlog::info("Desktop update check: {}, following the latest release", m.error());

                // A newer release is out and the matrix updates itself: have it update now, then follow it.
                if (matrix && latest && latest->version > matrix->version && matrix->autoUpdateEnabled)
                {
                    if (!matrix->busy)
                    {
                        if (auto r = UpdateChecker::requestMatrixUpdate(host, port, latest->version))
                            spdlog::info("Asked the matrix to update {} -> {}", v(matrix->version), v(latest->version));
                        else
                            spdlog::warn("Could not ask the matrix to update: {}", r.error());
                    }
                    {
                        std::lock_guard lock(state->mutex);
                        state->matrixWaitUntil = Clock::now() + MATRIX_WAIT_LIMIT;
                    }
                    finish("The matrix is updating to " + v(latest->version) +
                               ". The desktop app follows as soon as the matrix runs it.",
                           "Waiting for the matrix to update to " + v(latest->version));
                    return;
                }
            }

            Common::Version target;
            std::string reason;
            if (matrix)
            {
                target = matrix->version;
                reason = "This is the version your matrix runs. Matching versions are needed for desktop rendering.";
            }
            else if (latest)
            {
                if (!(latest->version > current))
                {
                    finish("You're running the latest version (" + v(current) + ").", "");
                    return;
                }
                target = latest->version;
                reason = "This is the latest release.";
            }
            else
            {
                finish("Could not check for updates: " + latest.error(), "Update check failed: " + latest.error());
                return;
            }

            if (target == current)
            {
                finish("You're up to date (" + v(current) + (matrix ? ", same as the matrix)." : ")."), "");
                return;
            }

            auto release = latest && latest->version == target ? latest : UpdateChecker::fetchRelease(target);
            if (!release)
            {
                finish("The matrix runs " + v(target) + ", which is not a published release (" + release.error() + ").",
                       "Matrix runs unreleased " + v(target));
                return;
            }
            if (release->downloadUrl.empty())
            {
                finish("Release " + v(target) + " has no package for this platform.", "");
                return;
            }

            if (SelfUpdate::is_managed_install() && (prefs.autoUpdate || manual))
            {
                spdlog::info("Updating desktop {} -> {} ({})", v(current), v(target), matrix ? "matrix version" : "latest release");
                install(state, *release, !manual);
                return;
            }
            if (!manual && !prefs.shouldRemindForVersion(release->tagName))
            {
                finish("", "Update to " + v(target) + " available");
                return;
            }
            std::lock_guard lock(state->mutex);
            state->release = *release;
            state->reason = reason;
            state->status = "Update to " + v(target) + " available";
            state->phase = Phase::Prompt;
        }

        int64_t nowSeconds()
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
    } // namespace

    struct UpdateManager::Impl
    {
        std::shared_ptr<State> state = std::make_shared<State>();
        UpdatePreferences prefs;
        Clock::time_point lastCheck{};
        bool checkedOnce = false;
        bool restartRequested = false;
        bool restartMinimized = false;

        // Old .deb / admin install left behind after moving to a per-user install.
        enum class Legacy { Unchecked, Ask, Removing, Done } legacy = Legacy::Unchecked;
        std::shared_ptr<std::atomic<int>> legacyResult = std::make_shared<std::atomic<int>>(-1);

        bool begin(bool manual)
        {
            {
                std::lock_guard lock(state->mutex);
                const Phase p = state->phase;
                if (p == Phase::Checking || p == Phase::Downloading || p == Phase::Ready || p == Phase::ReadyDismissed)
                    return false;
                if (!manual && p != Phase::Idle)
                    return false; // a dialog is open
                state->phase = Phase::Checking;
            }
            lastCheck = Clock::now();
            checkedOnce = true;
            std::thread([s = state, p = prefs, manual] {
                try
                {
                    sync(s, p, manual);
                }
                catch (const std::exception &e)
                {
                    spdlog::error("Desktop update check failed: {}", e.what());
                    std::lock_guard lock(s->mutex);
                    s->message = e.what();
                    s->phase = manual ? Phase::Result : Phase::Idle;
                }
            }).detach();
            return true;
        }
    };

    UpdateManager::UpdateManager() : pImpl(std::make_unique<Impl>()) { pImpl->prefs.load(); }

    UpdateManager::~UpdateManager() = default;

    void UpdateManager::checkForUpdatesAsync()
    {
        if (pImpl->prefs.updateNotificationsEnabled)
            pImpl->begin(false);
    }

    void UpdateManager::checkForUpdatesManual() { pImpl->begin(true); }

    void UpdateManager::tick(bool windowHidden)
    {
        const Phase phase = pImpl->state->get();
        if ((phase == Phase::Ready || phase == Phase::ReadyDismissed) && windowHidden && pImpl->prefs.autoUpdate &&
            !pImpl->restartRequested)
        {
            spdlog::info("Restarting in the background to finish the update");
            requestRestart(true);
            return;
        }
        if (phase != Phase::Idle || !pImpl->checkedOnce)
            return; // the first check comes from checkForUpdatesAsync() after startup

        Clock::time_point waitUntil;
        {
            std::lock_guard lock(pImpl->state->mutex);
            waitUntil = pImpl->state->matrixWaitUntil;
        }
        const auto now = Clock::now();
        const auto interval = now < waitUntil ? MATRIX_WAIT_INTERVAL : CHECK_INTERVAL;
        if (now - pImpl->lastCheck >= interval)
            checkForUpdatesAsync();
    }

    void UpdateManager::render(ImGuiContext *ctx)
    {
        ImGui::SetCurrentContext(ctx);
        auto &state = *pImpl->state;
        const Phase phase = state.get();
        ReleaseInfo release;
        std::string reason, message;
        bool silent;
        {
            std::lock_guard lock(state.mutex);
            release = state.release;
            reason = state.reason;
            message = state.message;
            silent = state.silent;
        }
        const auto modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

        if (phase == Phase::Prompt)
        {
            ImGui::OpenPopup("Update Available");
            if (ImGui::BeginPopupModal("Update Available", nullptr, modalFlags))
            {
                ImGui::Text("LED Matrix Controller %s is available (you have v%s).", ("v" + release.version.toString()).c_str(),
                            UpdateChecker::getCurrentVersion().toString().c_str());
                ImGui::TextWrapped("%s", reason.c_str());
                ImGui::Separator();
                const bool managed = SelfUpdate::is_managed_install();
                if (!managed)
                    ImGui::TextWrapped("Updating installs the app for your user account in %s (no %s needed). "
                                       "From then on it keeps itself up to date.",
                                       SelfUpdate::install_root().string().c_str(),
#ifdef _WIN32
                                       "administrator rights"
#else
                                       "root"
#endif
                    );
                ImGui::Spacing();
                if (ImGui::Button("Update Now", ImVec2(140, 0)))
                {
                    ImGui::CloseCurrentPopup();
                    state.set(Phase::Downloading);
                    std::thread([s = pImpl->state, release] { install(s, release, false); }).detach();
                }
                ImGui::SameLine();
                if (ImGui::Button("Remind Later", ImVec2(140, 0)))
                {
                    pImpl->prefs.lastRemindTime = nowSeconds();
                    pImpl->prefs.save();
                    state.set(Phase::Idle);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemTooltip("You'll be reminded again in %d days", pImpl->prefs.remindIntervalDays);
                ImGui::SameLine();
                if (ImGui::Button("Skip Version", ImVec2(140, 0)))
                {
                    pImpl->prefs.skippedVersion = release.tagName;
                    pImpl->prefs.lastRemindTime = nowSeconds();
                    pImpl->prefs.save();
                    state.set(Phase::Idle);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        else if ((phase == Phase::Downloading && !silent) || phase == Phase::Failed)
        {
            ImGui::OpenPopup("Updating");
            if (ImGui::BeginPopupModal("Updating", nullptr, modalFlags))
            {
                if (phase == Phase::Downloading)
                {
                    ImGui::Text("Downloading and installing v%s…", release.version.toString().c_str());
                    const size_t total = state.total.load(), done = state.downloaded.load();
                    const std::string label = total > 0 ? std::to_string(done >> 20) + " / " + std::to_string(total >> 20) + " MB"
                                                        : "Starting…";
                    ImGui::ProgressBar(total > 0 ? float(done) / float(total) : 0.0f, ImVec2(320.0f, 0.0f), label.c_str());
                    ImGui::TextDisabled("You can keep using the app meanwhile.");
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "The update failed:");
                    ImGui::TextWrapped("%s", message.c_str());
                    if (ImGui::Button("OK", ImVec2(120, 0)))
                    {
                        state.set(Phase::Idle);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
            }
        }
        else if (phase == Phase::Ready)
        {
            ImGui::OpenPopup("Update Installed");
            if (ImGui::BeginPopupModal("Update Installed", nullptr, modalFlags))
            {
                ImGui::Text("LED Matrix Controller v%s is installed.", release.version.toString().c_str());
                ImGui::TextDisabled("It also starts by itself the next time the app is hidden in the tray or restarted.");
                if (ImGui::Button("Restart Now", ImVec2(140, 0)))
                {
                    requestRestart(false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Later", ImVec2(140, 0)))
                {
                    state.set(Phase::ReadyDismissed);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        else if (phase == Phase::Result)
        {
            ImGui::OpenPopup("Software Update");
            if (ImGui::BeginPopupModal("Software Update", nullptr, modalFlags))
            {
                ImGui::TextWrapped("%s", message.c_str());
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    state.set(Phase::Idle);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        // Offer to remove the .deb / admin install we migrated away from (once per run).
        if (pImpl->legacy == Impl::Legacy::Unchecked)
            pImpl->legacy = SelfUpdate::is_managed_install() && SelfUpdate::legacy_install() ? Impl::Legacy::Ask : Impl::Legacy::Done;
        if (pImpl->legacy == Impl::Legacy::Removing && pImpl->legacyResult->load() >= 0)
            pImpl->legacy = Impl::Legacy::Done;
        if (pImpl->legacy == Impl::Legacy::Ask && phase == Phase::Idle)
        {
            ImGui::OpenPopup("Remove Old Installation");
            if (ImGui::BeginPopupModal("Remove Old Installation", nullptr, modalFlags))
            {
                const auto legacy = SelfUpdate::legacy_install();
                ImGui::TextWrapped("The app now runs from your user account and updates itself.\n"
                                   "The old copy at %s is no longer needed.",
                                   legacy ? legacy->string().c_str() : "?");
                if (ImGui::Button("Remove It", ImVec2(140, 0)))
                {
                    pImpl->legacy = Impl::Legacy::Removing;
                    std::thread([r = pImpl->legacyResult] { r->store(SelfUpdate::remove_legacy_install() ? 1 : 0); }).detach();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemTooltip(
#ifdef _WIN32
                    "Runs the old uninstaller (asks for administrator rights once)"
#else
                    "Runs 'pkexec dpkg --remove led-matrix-desktop' (asks for your password once)"
#endif
                );
                ImGui::SameLine();
                if (ImGui::Button("Keep It", ImVec2(140, 0)))
                {
                    SelfUpdate::forget_legacy_install();
                    pImpl->legacy = Impl::Legacy::Done;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }

    void UpdateManager::setUpdateNotificationsEnabled(bool enabled)
    {
        pImpl->prefs.updateNotificationsEnabled = enabled;
        pImpl->prefs.save();
    }

    bool UpdateManager::isUpdateNotificationsEnabled() const { return pImpl->prefs.updateNotificationsEnabled; }

    void UpdateManager::setAutoUpdateEnabled(bool enabled)
    {
        pImpl->prefs.autoUpdate = enabled;
        pImpl->prefs.save();
    }

    bool UpdateManager::isAutoUpdateEnabled() const { return pImpl->prefs.autoUpdate; }

    void UpdateManager::setFollowMatrixEnabled(bool enabled)
    {
        pImpl->prefs.followMatrix = enabled;
        pImpl->prefs.save();
    }

    bool UpdateManager::isFollowMatrixEnabled() const { return pImpl->prefs.followMatrix; }

    void UpdateManager::resetUpdatePreferences()
    {
        pImpl->prefs = UpdatePreferences{};
        pImpl->prefs.save();
        spdlog::info("Reset all update preferences to defaults");
    }

    UpdatePreferences &UpdateManager::getUpdatePreferences() { return pImpl->prefs; }

    std::string UpdateManager::pendingRestartVersion() const
    {
        std::lock_guard lock(pImpl->state->mutex);
        const Phase p = pImpl->state->phase;
        return p == Phase::Ready || p == Phase::ReadyDismissed ? pImpl->state->release.version.toString() : std::string{};
    }

    void UpdateManager::requestRestart(bool startMinimized)
    {
        if (pendingRestartVersion().empty())
            return;
        pImpl->restartRequested = true;
        pImpl->restartMinimized = startMinimized;
    }

    bool UpdateManager::shallAppExit() const { return pImpl->restartRequested; }
    bool UpdateManager::restartAfterExit() const { return pImpl->restartRequested; }
    bool UpdateManager::restartMinimized() const { return pImpl->restartMinimized; }

    std::string UpdateManager::statusLine() const
    {
        std::lock_guard lock(pImpl->state->mutex);
        return pImpl->state->status;
    }

} // namespace UpdateChecker
