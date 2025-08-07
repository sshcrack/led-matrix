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
        
        Common::Version current_version_;
        std::string repo_owner_;
        std::string repo_name_;
        
        // Callback function for when update is available/installed
        std::function<void(UpdateStatus, const std::string&)> status_callback_;
        
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
        explicit UpdateManager(Config::MainConfig* config, 
                             const Common::Version& current_version = Common::Version(),
                             const std::string& repo_owner = "sshcrack",
                             const std::string& repo_name = "led-matrix");
        
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
        
        // Configuration methods
        void set_auto_update_enabled(bool enabled);
        void set_check_interval_hours(int hours);
        bool is_auto_update_enabled() const;
        int get_check_interval_hours() const;
        
        // Update information
        bool is_update_available() const;
        Common::Version get_latest_version() const;
        Common::Version get_current_version() const;
        void set_current_version(const Common::Version& version);
        bool is_updates_supported() const;
    };
}