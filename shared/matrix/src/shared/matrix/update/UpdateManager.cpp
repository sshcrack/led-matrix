#include "shared/matrix/update/UpdateManager.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/interrupt.h"
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
#include <atomic>

#ifdef __linux__
#include <sys/utsname.h>
#endif

using namespace spdlog;
using namespace std;

namespace Update
{

    UpdateManager::UpdateManager(Config::MainConfig *config)
        : running_(false), should_stop_(false), status_(UpdateStatus::IDLE),
          config_(config), repo_owner_("sshcrack"), repo_name_("led-matrix")
    {

        // Check if the platform supports updates
        if (!is_updates_supported())
        {
            warn("Update system disabled - not running on supported platform");
            status_.store(UpdateStatus::DISABLED);
            return;
        }
    }

    UpdateManager::~UpdateManager()
    {
        stop();
    }

    void UpdateManager::start()
    {
        if (running_.load())
        {
            return;
        }

        should_stop_.store(false);
        running_.store(true);
        update_thread_ = thread(&UpdateManager::update_loop, this);

        info("UpdateManager started");
    }

    void UpdateManager::stop()
    {
        if (!running_.load())
        {
            return;
        }

        should_stop_.store(true);
        if (update_thread_.joinable())
        {
            update_thread_.join();
        }

        running_.store(false);
        info("UpdateManager stopped");
        // Launch update script if pending
        if (pending_update_.load() && !pending_update_filename_.empty())
        {
#ifdef ENABLE_UPDATE_TESTING
            // For testing, just set the success flag
            filesystem::path success_flag = get_exec_dir() / ".update_test_success";
            try_remove(success_flag);
            ofstream(success_flag).close();
            info("Update test success flag created at {}", success_flag.string());
            info("Normally, the UpdateManager is stopping here and running the update script at {}", pending_update_filename_);
            return;
#endif

            std::filesystem::path script_path = get_exec_dir() / "update_service.sh";
            string command = "sudo " + script_path.string() + " " + pending_update_filename_ + " &";
            int result = system(command.c_str());
            if (result != 0)
            {
                spdlog::error("Failed to launch update script: {}", command);
                return;
            }
            info("Update script launched. The process must exit now.");
        }
    }

    void UpdateManager::update_loop()
    {
        info("Getting latest version information...");
        latest_version_ = Common::Version::getCurrentVersion();
        if (should_check_for_updates())
        {
            auto latest_info = get_update_info();
            if (latest_info.has_value())
            {
                latest_version_ = latest_info->version;
                info("Latest version: {}", latest_version_.toString());
            }
            else
            {
                warn("Error fetching update info: {}", latest_info.error());
            }
        }

        while (!should_stop_.load())
        {
            if (!should_check_for_updates())
            {
                // Sleep for half an hour between checks
                for (int i = 0; i < 1800 && !should_stop_.load(); ++i)
                {
                    this_thread::sleep_for(chrono::seconds(1));
                }
                continue;
            }

            auto latest_info_opt = get_update_info();
            if (!latest_info_opt.has_value())
            {
                spdlog::warn("Failed to get update info: {}. Sleeping for 5 minutes...", latest_info_opt.error());
                for (int i = 0; i < 5 * 60 && !should_stop_.load(); ++i)
                {
                    this_thread::sleep_for(chrono::seconds(1));
                }

                continue;
            }

            auto latest_info = latest_info_opt.value();
            config_->set_last_check_time(GetTimeInMillis());
            config_->save();

            latest_version_ = latest_info.version;
            if (latest_version_ <= Common::Version::getCurrentVersion())
                continue;

            auto res = manual_download_and_install(latest_info);
            if (res.has_value())
            {
                info("Auto-update completed successfully");
            }
            else
            {
                error("Auto-update failed: {}", res.error());
            }

            set_last_check_time(GetTimeInMillis());
            config_->save();
        }
    }

    // Unified get_update_info method with optional<string> version
    expected<UpdateInfo, string> UpdateManager::get_update_info(const std::optional<Common::Version> &version)
    {
        try
        {
            string api_url = get_github_api_url(version);
            debug("Checking for updates at: {}", api_url);

            auto response = cpr::Get(
                cpr::Url{api_url},
                cpr::Header{{"User-Agent", get_user_agent()}});

            if (response.status_code != 200)
                return std::unexpected("GitHub API request failed with status: " + to_string(response.status_code));

            auto json_response = nlohmann::json::parse(response.text);
            string latest_version_str = json_response["tag_name"];

            // Remove 'v' prefix if present
            if (latest_version_str.starts_with("v"))
            {
                latest_version_str = latest_version_str.substr(1);
            }

            Common::Version latest_version = Common::Version::fromString(latest_version_str);

            // Find the led-matrix arm64 tar.gz asset
            string download_url = "";
            bool found_asset = false;

            if (json_response.contains("assets"))
            {
                for (const auto &asset : json_response["assets"])
                {
                    string asset_name = asset["name"];
                    if (asset_name.find("led-matrix") != string::npos &&
                        asset_name.find("arm64") != string::npos &&
                        asset_name.ends_with(".tar.gz"))
                    {
                        download_url = asset["browser_download_url"];
                        found_asset = true;
                        break;
                    }
                }
            }

            if (!found_asset)
                return std::unexpected("Could not find led-matrix arm64 asset in release");

            UpdateInfo info;
            info.version = version.value_or(latest_version);
            info.download_url = download_url;
            info.body = json_response.value("body", "");
            info.is_prerelease = json_response.value("prerelease", false);

            return info;
        }
        catch (const exception &ex)
        {
            return std::unexpected("Error checking for updates: " + string(ex.what()));
        }
    }

    std::expected<void, std::string> UpdateManager::download_update(const string &url, const string &filename)
    {
        try
        {
            status_.store(UpdateStatus::DOWNLOADING);
            info("Downloading update from: {}", url);

            ofstream file(filename, ios::binary);
            if (!file)
                return std::unexpected("Could not create file: " + filename);

            auto response = cpr::Download(file, cpr::Url{url});
            if (response.status_code != 200)
                return std::unexpected("Download failed with status: " + to_string(response.status_code));

            info("Download completed: {}", filename);
            return {};
        }
        catch (const exception &ex)
        {
            return std::unexpected("Error downloading update: " + string(ex.what()));
        }
    }

    std::expected<void, std::string> UpdateManager::install_update(const UpdateInfo &update, const string &filename)
    {
        try
        {
            status_.store(UpdateStatus::INSTALLING);
            info("Installing update from: {}", filename);

            // Step 1: Ensure the archive file exists
            if (!filesystem::exists(filename))
                return std::unexpected("Update archive not found at " + filename);

            // Step 2: Call pre-restart callback to allow sending response
            info("Preparing to start update process...");
            if (pre_restart_callback_)
            {
                pre_restart_callback_();
                // Give a small delay to ensure response is sent
                this_thread::sleep_for(chrono::milliseconds(500));
            }

            // Step 3: Prepare to launch the update script in stop()
            info("Preparing to launch update script in stop() and exit process...");
            pending_update_filename_ = filename;
            pending_update_.store(true);
            exit_canvas_update = true;
            interrupt_received = true;

            return {};
        }
        catch (const exception &ex)
        {
            return std::unexpected("Error installing update: " + string(ex.what()));
        }
    }

    bool UpdateManager::should_check_for_updates()
    {
#if !defined(ENABLE_UPDATE_TESTING) && defined(ENABLE_EMULATOR)
        return false; // Disable updates in emulator mode
#endif

        if (!is_auto_update_enabled())
            return false;

        tmillis_t now = GetTimeInMillis();
        tmillis_t last_check = config_->get_last_check_time();
        tmillis_t interval_ms = static_cast<tmillis_t>(get_check_interval_hours()) * 60 * 60 * 1000;

        return (now - last_check) >= interval_ms;
    }

    string UpdateManager::get_github_api_url(const std::optional<Common::Version> &version)
    {
        std::string base_url = "https://api.github.com/repos/" + repo_owner_ + "/" + repo_name_ + "/releases";
        if (version.has_value())
            return base_url + "/tags/v" + version.value().toString();

        return base_url + "/latest";
    }

    expected<UpdateInfo, string> UpdateManager::manual_check_for_updates()
    {
        auto latest_opt = get_update_info();
        if (!latest_opt.has_value())
        {
            return std::unexpected(latest_opt.error());
        }

        auto latest_info = latest_opt.value();
        if (latest_info.version <= Common::Version::getCurrentVersion())
        {
            return std::unexpected("No updates available");
        }

        return latest_info;
    }

    std::expected<void, std::string> UpdateManager::manual_download_and_install(const UpdateInfo &update_info)
    {
        try
        {
            string filename = "/tmp/led-matrix-update.tar.gz";

            auto download_res = download_update(update_info.download_url, filename);
            if (!download_res.has_value())
            {
                status_.store(UpdateStatus::ERROR);
                error("Download failed: {}", download_res.error());

                std::unique_lock lock(error_mutex_);
                error_message_ = "Download failed:" + download_res.error();
                return download_res;
            }

            auto install_res = install_update(update_info, filename);
            if (!install_res.has_value())
            {
                status_.store(UpdateStatus::ERROR);
                error("Installation failed: {}", install_res.error());

                std::unique_lock lock(error_mutex_);
                error_message_ = "Installation failed: " + install_res.error();
                return install_res;
            }

            return {};
        }
        catch (const exception &ex)
        {
            return std::unexpected("Manual update failed: " + string(ex.what()));
        }
    }

    UpdateStatus UpdateManager::get_status() const
    {
        return status_.load();
    }

    void UpdateManager::set_pre_restart_callback(function<void()> callback)
    {
        pre_restart_callback_ = callback;
    }

    void UpdateManager::set_auto_update_enabled(bool enabled)
    {
        config_->set_auto_update_enabled(enabled);
    }

    void UpdateManager::set_check_interval_hours(int hours)
    {
        config_->set_update_check_interval_hours(hours);
    }

    bool UpdateManager::is_auto_update_enabled() const
    {
        return config_->is_auto_update_enabled();
    }

    int UpdateManager::get_check_interval_hours() const
    {
        return config_->get_update_check_interval_hours();
    }

    bool UpdateManager::is_update_available() const
    {
        return Common::Version::getCurrentVersion() < latest_version_;
    }

    tmillis_t UpdateManager::get_last_check_time() const
    {
        return config_->get_last_check_time();
    }

    void UpdateManager::set_last_check_time(tmillis_t time)
    {
        config_->set_last_check_time(time);
    }

    std::optional<Common::Version> UpdateManager::get_latest_version() const
    {
        return latest_version_;
    }

    bool UpdateManager::is_platform_supported()
    {
#ifdef ENABLE_UPDATE_TESTING
        // Allow updates for testing purposes
        return true;
#endif

#ifdef ENABLE_EMULATOR
        // Disable updates when running in emulator mode
        return false;
#endif

#ifdef __linux__
        if (get_exec_dir() != std::filesystem::path("/opt/led-matrix/"))
            return false;

        // Check if running on ARM64 and Raspberry Pi
        struct utsname system_info;
        if (uname(&system_info) == 0)
        {
            string machine = system_info.machine;

            return machine == "aarch64" || machine == "arm64";
        }
#endif

        // Only support Linux and arm64 for automatic updates
        return false;
    }

    string UpdateManager::get_user_agent()
    {
        Common::Version current_version = Common::Version::getCurrentVersion();
        return "led-matrix/" + current_version.toString() + " (https://github.com/sshcrack/led-matrix)";
    }

    bool UpdateManager::is_updates_supported() const
    {
        return is_platform_supported() && !repo_owner_.empty() && !repo_name_.empty();
    }

    bool UpdateManager::check_and_handle_update_completion()
    {

#ifdef ENABLE_UPDATE_TESTING
        filesystem::path success_flag = get_exec_dir() / ".update_test_success";
#else
        filesystem::path success_flag = "/opt/led-matrix/.update_success";
#endif
        filesystem::path error_flag = "/opt/led-matrix/.update_error";

        // Check for update success flag
        if (filesystem::exists(success_flag))
        {
            info("Update completion detected - update was successful");

            // Read the timestamp from the flag file
            ifstream flag_file(success_flag);
            string timestamp;
            if (flag_file.good())
            {
                getline(flag_file, timestamp);
                flag_file.close();
            }

            // Remove the flag file
            filesystem::remove(success_flag);

            status_.store(UpdateStatus::SUCCESS);
            info("Update completed successfully at {}", timestamp);
            return true;
        }

        // Check for update error flag
        if (filesystem::exists(error_flag))
        {
            warn("Update completion detected - update failed");

            // Read the error message from the flag file
            ifstream flag_file(error_flag);
            string error_msg;
            if (flag_file.good())
            {
                getline(flag_file, error_msg);
                flag_file.close();
            }

            // Remove the flag file
            filesystem::remove(error_flag);

            // Update the status and trigger callback if set
            spdlog::warn("Update failed: {}", error_msg);
            status_.store(UpdateStatus::ERROR);

            std::unique_lock lock(error_mutex_);
            error_message_ = "Update failed: " + error_msg;
            return true;
        }

        return false;
    }
}