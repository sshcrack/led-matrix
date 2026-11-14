#ifdef _WIN32
#include "shared/common/win_compat.h"
#endif
#include <cpr/cpr.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>

#include "shared/common/Version.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/update/UpdateManager.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"

#ifdef __linux__
#include <sys/utsname.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <iterator>
#endif

using namespace spdlog;
using namespace std;

namespace Update
{
    namespace
    {
        // Shared between the (unprivileged) daemon and the root update unit; see led-matrix-update.path.
        std::filesystem::path update_request_dir() { return "/var/lib/led-matrix/update"; }

        std::mutex& state_dir_mutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        std::filesystem::path& state_dir_override()
        {
            static std::filesystem::path dir;
            return dir;
        }

        Releases::FetchOptions fetch_options()
        {
            return Releases::make_fetch_options("led-matrix", Common::Version::getCurrentVersion(), 30000);
        }

        void write_error_flag(const std::string& message)
        {
            std::error_code ec;
            std::filesystem::create_directories(UpdateManager::state_dir(), ec);
            std::ofstream(UpdateManager::state_dir() / ".update_error", std::ios::trunc) << message;
        }
        }  // namespace

        std::filesystem::path UpdateManager::state_dir()
        {
            std::lock_guard lock(state_dir_mutex());
            return state_dir_override().empty() ? std::filesystem::path("/var/lib/led-matrix") : state_dir_override();
        }

        void UpdateManager::set_state_dir_for_testing(std::filesystem::path dir)
        {
            std::lock_guard lock(state_dir_mutex());
            state_dir_override() = std::move(dir);
        }

    UpdateManager::UpdateManager(Config::MainConfig* mainConfig)
        : running_(false),
          should_stop_(false),
          status_(UpdateStatus::IDLE),
          config_(mainConfig),
          repo_owner_("sshcrack"),
          repo_name_("led-matrix")
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

#ifdef __linux__
            // A child in led-matrix.service would be killed when dpkg restarts
            // that service. A transient unit owns the complete update transaction.
            std::string port = getenv("PORT") ? getenv("PORT") : "8080";
            if (!std::regex_match(port, std::regex("[1-9][0-9]{0,4}")) || std::stoul(port) > 65535)
                port = "8080";

            // Preferred: hand the package to the root-owned led-matrix-update.path unit. It needs no privilege
            // escalation, so it works under NoNewPrivileges/ProtectSystem hardening where `sudo` is blocked.
            if (filesystem::exists("/lib/systemd/system/led-matrix-update.path") ||
                filesystem::exists("/etc/systemd/system/led-matrix-update.path"))
            {
                std::error_code request_ec;
                const auto request_dir = update_request_dir();
                filesystem::create_directories(request_dir, request_ec);
                const auto tmp_request = request_dir / ".request.tmp";
                const auto request = request_dir / "request";
                {
                    std::ofstream out(tmp_request, std::ios::trunc);
                    out << pending_update_filename_ << '\n' << port << '\n';
                    if (!out.flush())
                        request_ec = std::make_error_code(std::errc::io_error);
                }
                if (!request_ec)
                    filesystem::rename(tmp_request, request, request_ec);
                if (request_ec)
                {
                    write_error_flag("Failed to queue the update request: " + request_ec.message());
                    spdlog::error("Failed to queue the update request: {}", request_ec.message());
                    return;
                }
                info("Update request queued for led-matrix-update.path. The process must exit now.");
                return;
            }

            std::vector<std::string> arguments{
                "sudo", "-n", "systemd-run", "--collect", "--unit=led-matrix-update",
                "--property=Type=exec", "--setenv=LED_MATRIX_UPDATE_PORT=" + port,
                "/usr/lib/led-matrix/update_service.sh", pending_update_filename_
            };
            std::vector<char *> argv;
            for (auto &argument : arguments) argv.push_back(argument.data());
            argv.push_back(nullptr);
            pid_t child;
            const int spawned = posix_spawnp(&child, argv.front(), nullptr, nullptr, argv.data(), ::environ);
            int status = 0;
            pid_t waited = -1;
            if (spawned == 0) {
                do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
            }
            if (spawned != 0 || waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                write_error_flag("Failed to launch the update transaction unit");
                spdlog::error("Failed to launch the update transaction unit");
                return;
            }
#else
            spdlog::error("Transactional updates require Linux/systemd");
            return;
#endif
            info("Update script launched. The process must exit now.");
        }
    }

    void UpdateManager::set_latest_version(const Common::Version &version)
    {
        std::lock_guard lock(latest_version_mutex_);
        latest_version_ = version;
    }

    Common::Version UpdateManager::latest_version() const
    {
        std::lock_guard lock(latest_version_mutex_);
        return latest_version_;
    }

    void UpdateManager::update_loop()
    {
        info("Getting latest version information...");
        set_latest_version(Common::Version::getCurrentVersion());
        if (should_check_for_updates())
        {
            auto latest_info = get_update_info();
            if (latest_info.has_value())
            {
                set_latest_version(latest_info->version);
                info("Latest version: {}", latest_info->version.toString());
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

            set_latest_version(latest_info.version);
            if (latest_info.version <= Common::Version::getCurrentVersion())
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

    namespace
    {
    expected<UpdateInfo, string> to_update_info(const Releases::Release& release, const std::optional<Common::Version>& version)
    {
        auto asset = Releases::select_asset(release, Releases::is_matrix_package, "led-matrix arm64");
        if (!asset)
            return std::unexpected(asset.error());

        UpdateInfo info;
        info.version = version.value_or(release.version);
        info.download_url = asset->download_url;
        info.sha256 = asset->sha256;
        info.body = release.body;
        info.is_prerelease = release.prerelease;
        return info;
    }
    }  // namespace

    expected<UpdateInfo, string> UpdateManager::parse_release(const nlohmann::json& json, const std::optional<Common::Version>& version)
    {
        auto release = Releases::parse_release(json);
        if (!release)
            return std::unexpected(release.error());
        return to_update_info(*release, version);
    }

    // Unified get_update_info method with optional<string> version
    expected<UpdateInfo, string> UpdateManager::get_update_info(const std::optional<Common::Version> &version)
    {
        try
        {
            const auto options = fetch_options();
            debug("Checking for updates at: {}", Releases::releases_url(options));
            auto release = version ? Releases::fetch_version(*version, options) : Releases::fetch_latest(options);
            if (!release)
                return std::unexpected(release.error());
            return to_update_info(*release, version);
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
            if (!Releases::is_allowed_download_url(url))
                return std::unexpected("Refusing to download an update from " + url);
            status_.store(UpdateStatus::DOWNLOADING);
            info("Downloading update from: {}", url);

            ofstream file(filename, ios::binary);
            if (!file)
                return std::unexpected("Could not create file: " + filename);

            auto response = cpr::Download(file, cpr::Url{url}, cpr::ConnectTimeout{5000}, cpr::Timeout{180000},
                                          cpr::LowSpeed{1024, std::chrono::seconds{10}},
                                          cpr::ProgressCallback{[this](auto, auto, auto, auto, intptr_t) { return !should_stop_.load(); }});
            file.flush();
            file.close();
            if (response.error.code != cpr::ErrorCode::OK || response.status_code != 200 ||
                should_stop_.load() || !file) {
                try_remove(filename);
                return std::unexpected("Download failed or could not be written (HTTP " + to_string(response.status_code) + ")");
            }

            info("Download completed: {}", filename);
            return {};
        }
        catch (const exception &ex)
        {
            try_remove(filename);
            return std::unexpected("Error downloading update: " + string(ex.what()));
        }
    }

    std::expected<void, std::string> UpdateManager::install_update(const UpdateInfo&, const string& filename)
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
#else
        if (!is_auto_update_enabled())
            return false;

        tmillis_t now = GetTimeInMillis();
        tmillis_t last_check = config_->get_last_check_time();
        tmillis_t interval_ms = static_cast<tmillis_t>(get_check_interval_hours()) * 60 * 60 * 1000;

        return (now - last_check) >= interval_ms;
#endif
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
        std::unique_lock transaction_lock(installation_mutex_, std::try_to_lock);
        if (!transaction_lock.owns_lock() || pending_update_.load())
            return std::unexpected("An update is already in progress");
        // Never install an unverified package: no valid SHA-256 digest, no download. The refusal is
        // reported like any failed update, through the status and the `.update_error` flag.
        if (const auto digest = Releases::require_digest(update_info.sha256, "the led-matrix arm64 package"); !digest) {
            error("Refusing update {}: {}", update_info.version.toString(), digest.error());
            fail_install(digest.error());
            write_error_flag(digest.error());
            return std::unexpected(digest.error());
        }
        try
        {
            // Private 0700 directory: the root-run update script installs this file, so it must not live at a
            // predictable path in world-writable /tmp where another user could swap or symlink it.
#ifdef __linux__
            // Prefer the service's own writable data directory: with PrivateTmp the daemon's /tmp is invisible to the
            // root updater unit, so the package must live where both can see it.
            filesystem::path download_base = filesystem::temp_directory_path();
            {
                std::error_code base_ec;
                const filesystem::path shared_base = update_request_dir();
                filesystem::create_directories(shared_base, base_ec);
                if (!base_ec && access(shared_base.c_str(), W_OK) == 0)
                    download_base = shared_base;
            }
            string dir_template = (download_base / "led-matrix-update-XXXXXX").string();
            if (!mkdtemp(dir_template.data()))
                return std::unexpected("Could not create private update directory: " + string(strerror(errno)));
            string filename = dir_template + "/led-matrix-update.deb";
#else
            // Updates are only installed on Linux; keep other platforms compiling.
            const auto dir = filesystem::temp_directory_path() / "led-matrix-update";
            filesystem::create_directories(dir);
            string filename = (dir / "led-matrix-update.deb").string();
#endif

            auto download_res = download_update(update_info.download_url, filename);
            if (download_res.has_value())
            {
                download_res = Releases::verify_sha256(filename, update_info.sha256);
                if (download_res.has_value())
                    info("Verified update SHA-256: {}", update_info.sha256);
                else
                    try_remove(filename);
            }
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
            fail_install("Manual update failed: " + string(ex.what()));
            return std::unexpected("Manual update failed: " + string(ex.what()));
        }
    }

    UpdateStatus UpdateManager::get_status() const
    {
        return status_.load();
    }

    void UpdateManager::begin_manual_install()
    {
        {
            std::unique_lock lock(error_mutex_);
            error_message_.clear();
        }
        status_.store(UpdateStatus::CHECKING);
    }

    void UpdateManager::fail_install(const std::string &message)
    {
        {
            std::unique_lock lock(error_mutex_);
            error_message_ = message;
        }
        status_.store(UpdateStatus::ERROR);
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
        return Common::Version::getCurrentVersion() < latest_version();
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
        return latest_version();
    }

    bool UpdateManager::is_platform_supported()
    {
#if defined(ENABLE_UPDATE_TESTING)
        // Allow updates for testing purposes
        return true;
#elif defined(ENABLE_EMULATOR)
        // Disable updates when running in emulator mode
        return false;
#elif defined(__linux__)
        // Only support Linux and arm64 for automatic updates
        struct utsname system_info;
        if (uname(&system_info) != 0)
            return false;

        string machine = system_info.machine;
        return machine == "aarch64" || machine == "arm64";
#else
        return false;
#endif
    }

    bool UpdateManager::is_updates_supported() const
    {
        return is_platform_supported() && !repo_owner_.empty() && !repo_name_.empty();
    }

    bool UpdateManager::check_and_handle_update_completion()
    {
        std::lock_guard completion_lock(completion_mutex_);

#ifdef ENABLE_UPDATE_TESTING
        filesystem::path success_flag = get_exec_dir() / ".update_test_success";
#else
        filesystem::path success_flag = state_dir() / ".update_success";
#endif
        filesystem::path error_flag = state_dir() / ".update_error";

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