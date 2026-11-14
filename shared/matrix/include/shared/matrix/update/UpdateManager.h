#pragma once

#include <atomic>
#include <expected>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>

#include "nlohmann/json.hpp"
#include "shared/common/Releases.h"
#include "shared/common/Version.h"
#include "shared/matrix/config/MainConfig.h"

namespace Update
{
    struct UpdateInfo
    {
        Common::Version version;
        std::string download_url;
        // Lowercase hex SHA-256 from the GitHub asset "digest" field; empty for releases without one.
        std::string sha256;
        std::string body;
        bool is_prerelease;
    };

    /// Shared with the desktop (JSON names, legacy int values): see shared/common/Releases.h.
    using UpdateStatus = Releases::UpdateStatus;

    class UpdateManager
    {
    private:
        std::mutex installation_mutex_;
        std::mutex completion_mutex_;
        std::atomic<bool> running_;
        std::atomic<bool> should_stop_;
        std::atomic<UpdateStatus> status_;
        std::string error_message_;
        std::shared_mutex error_mutex_;
        std::thread update_thread_;
        Config::MainConfig *config_;
        // Written by update_loop() on the update thread, read by HTTP handlers.
        // Version is a multi-word struct, so guard it (torn reads otherwise).
        mutable std::mutex latest_version_mutex_;
        Common::Version latest_version_;
        void set_latest_version(const Common::Version &version);
        Common::Version latest_version() const;

        std::string repo_owner_;
        std::string repo_name_;
        std::function<void()> pre_restart_callback_;

        // Added for deferred update script launch
        std::atomic<bool> pending_update_{false};
        std::string pending_update_filename_;

        void update_loop();

        std::expected<void, std::string> download_update(const std::string& url, const std::string& filename);
        std::expected<void, std::string> install_update(const UpdateInfo &info, const std::string &filename);
        bool should_check_for_updates();
        
        static bool is_platform_supported();

    public:
        explicit UpdateManager(Config::MainConfig *config);

        ~UpdateManager();

        // Start/stop the update manager
        void start();
        void stop();

        /// Parses a GitHub release JSON object into UpdateInfo through Releases::parse_release /
        /// select_asset. Tolerates null or missing optional fields (body, prerelease, digest), which
        /// GitHub returns for releases without notes. A release without a digest parses (sha256 stays
        /// empty); manual_download_and_install refuses to install it. `version` overrides the release's
        /// tag when set.
        static std::expected<UpdateInfo, std::string> parse_release(const nlohmann::json &release,
                                                                     const std::optional<Common::Version> &version = std::nullopt);

        // Manual update operations
        std::expected<UpdateInfo, std::string> manual_check_for_updates();
        std::expected<UpdateInfo, std::string> get_update_info(const std::optional<Common::Version> &version = std::nullopt);
        std::expected<void, std::string> manual_download_and_install(const UpdateInfo &update_info);

        /// Directory of the `.update_success` / `.update_error` flags. /var/lib/led-matrix, except in tests.
        static std::filesystem::path state_dir();
        static void set_state_dir_for_testing(std::filesystem::path dir);

        UpdateStatus get_status() const;
        /// A manual install was accepted: report CHECKING and drop the previous outcome, so pollers never
        /// mistake an earlier ERROR/SUCCESS for the result of this install.
        void begin_manual_install();
        /// Records why an install failed (status ERROR + message).
        void fail_install(const std::string &message);
        void set_pre_restart_callback(std::function<void()> callback);

        // Configuration methods
        void set_auto_update_enabled(bool enabled);
        void set_check_interval_hours(int hours);
        bool is_auto_update_enabled() const;
        int get_check_interval_hours() const;

        // Update information
        bool is_update_available() const;
        std::optional<Common::Version> get_latest_version() const;
        std::string get_error_message() {
            std::shared_lock lock(error_mutex_);
            return error_message_;
        }

        bool is_updates_supported() const;

        // Update state management
        tmillis_t get_last_check_time() const;
        void set_last_check_time(tmillis_t time);

        // Update completion checking
        bool check_and_handle_update_completion();
    };
}