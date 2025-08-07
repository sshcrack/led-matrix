#include "shared/matrix/update/UpdateManager.h"
#include "shared/matrix/utils/shared.h"
#include "shared/common/Version.h"
#include "spdlog/spdlog.h"
#include <cpr/cpr.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <algorithm>
#include <regex>

#ifdef __linux__
#include <sys/utsname.h>
#endif

using namespace spdlog;
using namespace std;

namespace Update {
    
    UpdateManager::UpdateManager(Config::MainConfig* config, 
                               const string& repo_owner,
                               const string& repo_name)
        : running_(false), should_stop_(false), status_(UpdateStatus::IDLE),
          config_(config), repo_owner_(repo_owner), repo_name_(repo_name) {
        
        // Check if the platform supports updates
        if (!is_platform_supported()) {
            status_ = UpdateStatus::DISABLED;
            warn("Update system disabled - not running on supported platform");
            return;
        }
    }

    UpdateManager::~UpdateManager() {
        stop();
    }

    void UpdateManager::start() {
        if (running_.load()) {
            return;
        }
        
        should_stop_.store(false);
        running_.store(true);
        update_thread_ = thread(&UpdateManager::update_loop, this);
        
        info("UpdateManager started");
    }

    void UpdateManager::stop() {
        if (!running_.load()) {
            return;
        }
        
        should_stop_.store(true);
        
        if (update_thread_.joinable()) {
            update_thread_.join();
        }
        
        running_.store(false);
        info("UpdateManager stopped");
    }

    void UpdateManager::update_loop() {
        while (!should_stop_.load()) {
            try {
                if (should_check_for_updates()) {
                    auto update_info = check_for_updates();
                    
                    if (update_info.has_value()) {
                        config_->set_update_available(true);
                        config_->set_latest_version(update_info->version.toString());
                        config_->set_update_download_url(update_info->download_url);
                        
                        if (status_callback_) {
                            status_callback_(UpdateStatus::SUCCESS, 
                                           "Update available: " + update_info->version.toString());
                        }
                        
                        // If auto-update is enabled, download and install
                        if (is_auto_update_enabled()) {
                            if (manual_download_and_install(update_info->version.toString())) {
                                info("Auto-update completed successfully");
                            } else {
                                error("Auto-update failed");
                            }
                        }
                    } else {
                        config_->set_update_available(false);
                        Common::Version current_version(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
                        config_->set_latest_version(current_version.toString());
                        debug("No updates available");
                    }
                    
                    config_->set_last_check_time(GetTimeInMillis());
                    config_->save();
                }
                
                // Sleep for 1 hour between checks
                for (int i = 0; i < 3600 && !should_stop_.load(); ++i) {
                    this_thread::sleep_for(chrono::seconds(1));
                }
                
            } catch (const exception& ex) {
                set_error("Update loop error: " + string(ex.what()));
                error("Update loop error: {}", ex.what());
                
                // Sleep for 5 minutes on error before retrying
                for (int i = 0; i < 300 && !should_stop_.load(); ++i) {
                    this_thread::sleep_for(chrono::seconds(1));
                }
            }
        }
    }

    optional<UpdateInfo> UpdateManager::check_for_updates() {
        status_.store(UpdateStatus::CHECKING);
        
        try {
            string api_url = get_github_api_url();
            debug("Checking for updates at: {}", api_url);
            
            auto response = cpr::Get(
                cpr::Url{api_url},
                cpr::Header{{"User-Agent", get_user_agent()}}
            );
            
            if (response.status_code != 200) {
                set_error("GitHub API request failed with status: " + to_string(response.status_code));
                status_.store(UpdateStatus::ERROR);
                return nullopt;
            }
            
            auto json_response = nlohmann::json::parse(response.text);
            string latest_version_str = json_response["tag_name"];
            
            // Remove 'v' prefix if present
            if (latest_version_str.starts_with("v")) {
                latest_version_str = latest_version_str.substr(1);
            }
            
            Common::Version latest_version = Common::Version::fromString(latest_version_str);
            
            Common::Version current_version(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
            if (compare_versions(current_version, latest_version)) {
                // Find the led-matrix Linux tar.gz asset
                string download_url = "";
                bool found_asset = false;
                
                if (json_response.contains("assets")) {
                    for (const auto& asset : json_response["assets"]) {
                        string asset_name = asset["name"];
                        if (asset_name.find("led-matrix") != string::npos && 
                            asset_name.find("Linux") != string::npos &&
                            asset_name.ends_with(".tar.gz")) {
                            download_url = asset["browser_download_url"];
                            found_asset = true;
                            break;
                        }
                    }
                }
                
                if (!found_asset) {
                    set_error("Could not find led-matrix Linux asset in release");
                    status_.store(UpdateStatus::ERROR);
                    return nullopt;
                }
                
                UpdateInfo info;
                info.version = latest_version;
                info.download_url = download_url;
                info.body = json_response.value("body", "");
                info.is_prerelease = json_response.value("prerelease", false);
                
                status_.store(UpdateStatus::IDLE);
                return info;
            }
            
            status_.store(UpdateStatus::IDLE);
            return nullopt;
            
        } catch (const exception& ex) {
            set_error("Error checking for updates: " + string(ex.what()));
            status_.store(UpdateStatus::ERROR);
            return nullopt;
        }
    }

    bool UpdateManager::download_update(const string& url, const string& filename) {
        status_.store(UpdateStatus::DOWNLOADING);
        
        try {
            info("Downloading update from: {}", url);
            
            ofstream file(filename, ios::binary);
            if (!file) {
                set_error("Could not create file: " + filename);
                status_.store(UpdateStatus::ERROR);
                return false;
            }
            
            auto response = cpr::Get(cpr::Url{url});
            
            if (response.status_code != 200) {
                set_error("Download failed with status: " + to_string(response.status_code));
                status_.store(UpdateStatus::ERROR);
                return false;
            }
            
            file.write(response.text.c_str(), response.text.size());
            file.close();
            
            info("Download completed: {}", filename);
            return true;
            
        } catch (const exception& ex) {
            set_error("Error downloading update: " + string(ex.what()));
            status_.store(UpdateStatus::ERROR);
            return false;
        }
    }

    bool UpdateManager::install_update(const string& filename) {
        status_.store(UpdateStatus::INSTALLING);
        
        try {
            info("Installing update from: {}", filename);
            
            // Create a backup of config.json if it exists
            string config_backup = "/tmp/config.json.bak";
            if (filesystem::exists("/opt/led-matrix/config.json")) {
                filesystem::copy_file("/opt/led-matrix/config.json", config_backup);
            }
            
            // Extract the update to /opt/led-matrix
            string extract_cmd = "sudo tar -xzf " + filename + " -C /opt/led-matrix --strip-components=1";
            int result = system(extract_cmd.c_str());
            
            if (result != 0) {
                set_error("Failed to extract update archive");
                status_.store(UpdateStatus::ERROR);
                return false;
            }
            
            // Restore config.json if it was backed up
            if (filesystem::exists(config_backup)) {
                filesystem::copy_file(config_backup, "/opt/led-matrix/config.json");
                filesystem::remove(config_backup);
            }
            
            // Update the current version in config
            auto update_settings = config_->get_update_settings();
            string new_version = config_->get_latest_version();
            update_settings.last_update_time = GetTimeInMillis();
            update_settings.update_available = false;
            config_->set_update_settings(update_settings);
            config_->save();
            
            // Restart the service
            info("Restarting led-matrix service...");
            int restart_result = system("sudo systemctl restart led-matrix.service");
            if (restart_result != 0) {
                warn("Failed to restart led-matrix service, manual restart may be required");
            }
            
            status_.store(UpdateStatus::SUCCESS);
            info("Update installed successfully");
            
            // Clean up downloaded file
            filesystem::remove(filename);
            
            return true;
            
        } catch (const exception& ex) {
            set_error("Error installing update: " + string(ex.what()));
            status_.store(UpdateStatus::ERROR);
            return false;
        }
    }

    bool UpdateManager::should_check_for_updates() {
        if (!is_auto_update_enabled()) {
            return false;
        }
        
        tmillis_t now = GetTimeInMillis();
        tmillis_t last_check = config_->get_last_check_time();
        tmillis_t interval_ms = static_cast<tmillis_t>(get_check_interval_hours()) * 60 * 60 * 1000;
        
        return (now - last_check) >= interval_ms;
    }

    void UpdateManager::set_error(const string& error) {
        lock_guard<mutex> lock(error_mutex_);
        error_message_ = error;
    }

    string UpdateManager::get_github_api_url() {
        return "https://api.github.com/repos/" + repo_owner_ + "/" + repo_name_ + "/releases/latest";
    }

    bool UpdateManager::compare_versions(const Common::Version& current, const Common::Version& latest) {
        return latest > current;
    }

    optional<UpdateInfo> UpdateManager::manual_check_for_updates() {
        return check_for_updates();
    }

    bool UpdateManager::manual_download_and_install(const string& version) {
        try {
            string filename = "/tmp/led-matrix-update.tar.gz";
            string download_url = config_->get_update_download_url();
            
            if (download_url.empty()) {
                // If no URL stored, check for updates first
                auto update_info = check_for_updates();
                if (!update_info.has_value()) {
                    set_error("No update available");
                    return false;
                }
                download_url = update_info->download_url;
            }
            
            if (!download_update(download_url, filename)) {
                return false;
            }
            
            if (!install_update(filename)) {
                return false;
            }
            
            return true;
            
        } catch (const exception& ex) {
            set_error("Manual update failed: " + string(ex.what()));
            return false;
        }
    }

    UpdateStatus UpdateManager::get_status() const {
        return status_.load();
    }

    string UpdateManager::get_error_message() const {
        lock_guard<mutex> lock(const_cast<mutex&>(error_mutex_));
        return error_message_;
    }

    void UpdateManager::set_status_callback(function<void(UpdateStatus, const string&)> callback) {
        status_callback_ = callback;
    }

    void UpdateManager::set_auto_update_enabled(bool enabled) {
        config_->set_auto_update_enabled(enabled);
    }

    void UpdateManager::set_check_interval_hours(int hours) {
        config_->set_update_check_interval_hours(hours);
    }

    bool UpdateManager::is_auto_update_enabled() const {
        return config_->is_auto_update_enabled();
    }

    int UpdateManager::get_check_interval_hours() const {
        return config_->get_update_check_interval_hours();
    }

    bool UpdateManager::is_update_available() const {
        return config_->is_update_available();
    }

    Common::Version UpdateManager::get_latest_version() const {
        return Common::Version::fromString(config_->get_latest_version());
    }

    Common::Version UpdateManager::get_current_version() const {
        return Common::Version(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
    }



    bool UpdateManager::is_platform_supported() {
#ifdef ENABLE_UPDATE_TESTING
        // Allow updates for testing purposes
        return true;
#endif

#ifdef ENABLE_EMULATOR
        // Disable updates when running in emulator mode
        return false;
#endif

#ifdef __linux__
        // Check if running on ARM64 and Raspberry Pi
        struct utsname system_info;
        if (uname(&system_info) == 0) {
            string machine = system_info.machine;
            
            // Check for ARM64 architecture
            if (machine != "aarch64" && machine != "arm64") {
                return false;
            }
            
            // Check if it's a Raspberry Pi by looking for device tree info
            filesystem::path dt_model_path = "/proc/device-tree/model";
            if (filesystem::exists(dt_model_path)) {
                ifstream dt_model_file(dt_model_path);
                string model_info;
                getline(dt_model_file, model_info);
                
                // Check if it contains "Raspberry Pi"
                if (model_info.find("Raspberry Pi") != string::npos) {
                    return true;
                }
            }
        }
        return false;
#else
        // Only support Linux for automatic updates
        return false;
#endif
    }

    string UpdateManager::get_user_agent() {
        Common::Version current_version(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
        return "led-matrix/" + current_version.toString() + " (https://github.com/sshcrack/led-matrix)";
    }

    bool UpdateManager::is_updates_supported() const {
        return status_ != UpdateStatus::DISABLED;
    }
}