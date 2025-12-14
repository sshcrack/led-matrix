#pragma once

#include <string>
#include <vector>
#include <optional>
#include <expected>
#include <nlohmann/json.hpp>
#include "shared/common/Version.h"

namespace Plugins {
    struct PluginRelease {
        std::string version;
        std::string tag_name;
        std::string download_url;
        std::string name;
        std::string body;
        bool prerelease;
        std::string published_at;
        size_t size;
    };

    class PluginStore {
    public:
        static PluginStore& instance();

        // Get releases from a specific GitHub repository
        std::expected<std::vector<PluginRelease>, std::string> get_releases(const std::string& owner, const std::string& repo);

        // Download and install a plugin from a URL
        // Returns empty on success, error message on failure
        std::expected<void, std::string> install_plugin(const std::string& url, const std::string& filename);

        // Remove a plugin (by filename/directory name)
        std::expected<void, std::string> remove_plugin(const std::string& name);

    private:
        PluginStore() = default;
        ~PluginStore() = default;

        PluginStore(const PluginStore&) = delete;
        PluginStore& operator=(const PluginStore&) = delete;

        std::string get_github_api_url(const std::string& owner, const std::string& repo);
    };
}
