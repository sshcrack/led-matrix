#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <optional>
#include <functional>
#include "shared/matrix/config/MainConfig.h"
#include "shared/common/Version.h"
#include "nlohmann/json.hpp"

namespace Update {
    struct UpdateInfo {
        Common::Version version;
        std::string download_url;
        std::string body;
        bool is_prerelease;
    };

    enum class UpdateStatus {
        IDLE,
        CHECKING,
        DOWNLOADING,
        INSTALLING,
        ERROR,
        SUCCESS,
        DISABLED
    };

    class UpdateManager {
    private:
        std::atomic<bool> running_;
        std::atomic<bool> should_stop_;
        std::atomic<UpdateStatus> status_;
        std::string error_message_;
        std::mutex error_mutex_;
        std::thread update_thread_;
        Config::MainConfig* config_;
        std::string repo_owner_;
        std::string repo_name_;
        std::function<void(UpdateStatus, const std::string&)> status_callback_;
        std::function<void()> pre_restart_callback_;
        // Added for deferred update script launch
        std::atomic<bool> pending_update_{false};
        std::string pending_update_filename_;
        
        void update_loop();
        std::optional<UpdateInfo> check_for_updates();
        bool download_update(const std::string& url, const std::string& filename);
        bool install_update(const std::string& filename);
        bool should_check_for_updates();
        void set_error(const std::string& error);
        std::string get_github_api_url();
        bool compare_versions(const Common::Version& current, const Common::Version& latest);
        bool is_platform_supported();
        std::string get_user_agent();
        
    public:
        explicit UpdateManager(Config::MainConfig* config);
        
        ~UpdateManager();
        
        // Start/stop the update manager
        void start();
        void stop();
        
        // Manual update operations
        std::optional<UpdateInfo> manual_check_for_updates();
        bool manual_download_and_install(const std::string& version = "");
        
        // Status and configuration
        UpdateStatus get_status() const;
        std::string get_error_message() const;
        void set_status_callback(std::function<void(UpdateStatus, const std::string&)> callback);
        void set_pre_restart_callback(std::function<void()> callback);
        
        // Configuration methods
        void set_auto_update_enabled(bool enabled);
        void set_check_interval_hours(int hours);
        bool is_auto_update_enabled() const;
        int get_check_interval_hours() const;
        
        // Update information
        bool is_update_available() const;
        void set_update_available(bool available);
        Common::Version get_latest_version() const;
        std::string get_latest_version_string() const;
        void set_latest_version_string(const std::string& version);
        bool is_updates_supported() const;
        
        // Update state management
        tmillis_t get_last_check_time() const;
        void set_last_check_time(tmillis_t time);
        std::string get_update_download_url() const;
        void set_update_download_url(const std::string& url);
        
        // Update completion checking
        bool check_and_handle_update_completion();
    };
}