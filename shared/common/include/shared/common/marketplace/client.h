#pragma once

#include "types.h"
#include <future>
#include <functional>
#include <memory>

namespace Plugins {
namespace Marketplace {

    class MarketplaceClient {
    public:
        using ProgressCallback = std::function<void(const InstallationProgress&)>;
        using CompletionCallback = std::function<void(bool success, const std::string& error_message)>;

        virtual ~MarketplaceClient() = default;

        // Index management
        virtual std::future<std::optional<MarketplaceIndex>> fetch_index(
            const std::string& index_url = "") = 0;
        virtual std::optional<MarketplaceIndex> get_cached_index() const = 0;
        virtual void update_index_cache(const MarketplaceIndex& index) = 0;

        // Plugin management
        virtual std::vector<InstalledPlugin> get_installed_plugins() const = 0;
        virtual InstallationStatus get_plugin_status(const std::string& plugin_id) const = 0;
        
        // Installation operations
        virtual std::future<bool> install_plugin(
            const PluginInfo& plugin,
            const std::string& version,
            ProgressCallback progress_cb = nullptr,
            CompletionCallback completion_cb = nullptr) = 0;
            
        virtual std::future<bool> uninstall_plugin(
            const std::string& plugin_id,
            CompletionCallback completion_cb = nullptr) = 0;
            
        virtual std::future<bool> update_plugin(
            const std::string& plugin_id,
            const std::string& new_version,
            ProgressCallback progress_cb = nullptr,
            CompletionCallback completion_cb = nullptr) = 0;

        // Plugin state management
        virtual bool enable_plugin(const std::string& plugin_id) = 0;
        virtual bool disable_plugin(const std::string& plugin_id) = 0;
        
        // Utility functions
        virtual bool verify_plugin_integrity(const std::string& plugin_path, const std::string& expected_sha512) = 0;
        virtual std::vector<std::string> resolve_dependencies(const PluginInfo& plugin) const = 0;

    protected:
        // Security functions
        virtual std::string calculate_sha512(const std::string& file_path) = 0;
        virtual bool download_file(const std::string& url, const std::string& destination,
                                 ProgressCallback progress_cb = nullptr) = 0;
    };

    // Factory function to create platform-specific implementations
    std::unique_ptr<MarketplaceClient> create_marketplace_client(const std::string& plugin_dir);

} // namespace Marketplace
} // namespace Plugins