#pragma once

#include <thread>
#include <atomic>
#include <shared_mutex>
#include <string>
#include <optional>
#include <functional>
#include <expected>
#include "shared/matrix/config/MainConfig.h"
#include "shared/common/Version.h"
#include "nlohmann/json.hpp"

namespace Update
{
    struct UpdateInfo
    {
        Common::Version version;
        std::string download_url;
        std::string body;
        bool is_prerelease;
    };

    enum class UpdateStatus
    {
        IDLE,
        CHECKING,
        DOWNLOADING,
        INSTALLING,
        ERROR,
        SUCCESS
    };

    class UpdateManager
    {
    private:
        std::atomic<bool> running_;
        std::atomic<bool> should_stop_;
        std::atomic<UpdateStatus> status_;
        std::string error_message_;
        std::shared_mutex error_mutex_;
        std::thread update_thread_;
        Config::MainConfig *config_;
        Common::Version latest_version_;

        std::string repo_owner_;
        std::string repo_name_;
        std::function<void()> pre_restart_callback_;

        // Added for deferred update script launch
        std::atomic<bool> pending_update_{false};
        std::string pending_update_filename_;

        void update_loop();

        std::expected<void, std::string> download_update(const std::string &url, const std::string &filename);
        std::expected<void, std::string> install_update(const UpdateInfo &info, const std::string &filename);
        bool should_check_for_updates();
        
        static bool is_platform_supported();

        std::string get_github_api_url(const std::optional<Common::Version> &version = std::nullopt);
        std::string get_user_agent();
    public:
        explicit UpdateManager(Config::MainConfig *config);

        ~UpdateManager();

        // Start/stop the update manager
        void start();
        void stop();

        // Manual update operations
        expected<UpdateInfo, string> manual_check_for_updates();
        expected<UpdateInfo, string> get_update_info(const std::optional<Common::Version> &version = std::nullopt);
        std::expected<void, std::string> manual_download_and_install(const UpdateInfo &update_info);

        UpdateStatus get_status() const;
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