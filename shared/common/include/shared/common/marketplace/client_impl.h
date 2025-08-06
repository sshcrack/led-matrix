#pragma once

#include "shared/common/marketplace/client.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <atomic>
#include <cpr/cpr.h>
#include "picosha2.h"

namespace Plugins {
namespace Marketplace {

    class MarketplaceClientImpl : public MarketplaceClient {
    private:
        std::string plugin_dir_;
        std::string cache_dir_;
        std::optional<MarketplaceIndex> cached_index_;
        std::vector<InstalledPlugin> installed_plugins_;
        mutable std::mutex cache_mutex_;
        mutable std::mutex installed_plugins_mutex_;
        std::atomic<bool> is_installing_{false};

        // Default marketplace index URL - can be overridden
        static constexpr const char* DEFAULT_INDEX_URL = 
            "https://raw.githubusercontent.com/led-matrix-plugins/marketplace/main/index.json";

    public:
        explicit MarketplaceClientImpl(const std::string& plugin_dir);
        ~MarketplaceClientImpl() override;

        // MarketplaceClient interface implementation
        std::future<std::optional<MarketplaceIndex>> fetch_index(
            const std::string& index_url = "") override;
        std::optional<MarketplaceIndex> get_cached_index() const override;
        void update_index_cache(const MarketplaceIndex& index) override;

        std::vector<InstalledPlugin> get_installed_plugins() const override;
        InstallationStatus get_plugin_status(const std::string& plugin_id) const override;
        
        std::future<bool> install_plugin(
            const PluginInfo& plugin,
            const std::string& version,
            ProgressCallback progress_cb = nullptr,
            CompletionCallback completion_cb = nullptr) override;
            
        std::future<bool> uninstall_plugin(
            const std::string& plugin_id,
            CompletionCallback completion_cb = nullptr) override;
            
        std::future<bool> update_plugin(
            const std::string& plugin_id,
            const std::string& new_version,
            ProgressCallback progress_cb = nullptr,
            CompletionCallback completion_cb = nullptr) override;

        bool enable_plugin(const std::string& plugin_id) override;
        bool disable_plugin(const std::string& plugin_id) override;
        
        bool verify_plugin_integrity(const std::string& plugin_path, const std::string& expected_sha512) override;
        std::vector<std::string> resolve_dependencies(const PluginInfo& plugin) const override;

    protected:
        std::string calculate_sha512(const std::string& file_path) override;
        bool download_file(const std::string& url, const std::string& destination,
                         ProgressCallback progress_cb = nullptr) override;

    private:
        void load_installed_plugins();
        void save_installed_plugins();
        bool create_directories();
        std::string get_plugin_install_path(const std::string& plugin_id) const;
        std::string get_cache_file_path() const;
        std::string get_installed_plugins_file_path() const;
        bool is_version_compatible(const std::string& required, const std::string& actual) const;
        
        // Installation helpers
        bool install_plugin_binary(const BinaryInfo& binary, const std::string& plugin_id, 
                                  const std::string& binary_type, ProgressCallback progress_cb);
        bool cleanup_failed_installation(const std::string& plugin_id);
        void notify_progress(const ProgressCallback& callback, const std::string& plugin_id, 
                           InstallationStatus status, double progress, 
                           const std::string& error = "");
    };

} // namespace Marketplace
} // namespace Plugins