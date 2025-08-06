#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace Plugins {
namespace Marketplace {

    struct SceneInfo {
        std::string name;
        std::string description;
        
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(SceneInfo, name, description)
    };

    struct BinaryInfo {
        std::string url;
        std::string sha512;
        size_t size;
        
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BinaryInfo, url, sha512, size)
    };

    struct ReleaseInfo {
        std::optional<BinaryInfo> matrix;
        std::optional<BinaryInfo> desktop;
        
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ReleaseInfo, matrix, desktop)
    };

    struct CompatibilityInfo {
        std::optional<std::string> matrix_version;
        std::optional<std::string> desktop_version;
        
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CompatibilityInfo, matrix_version, desktop_version)
    };

    struct PluginInfo {
        std::string id;
        std::string name;
        std::string description;
        std::string version;
        std::string author;
        std::vector<std::string> tags;
        std::optional<std::string> image;
        std::vector<SceneInfo> scenes;
        std::unordered_map<std::string, ReleaseInfo> releases;
        std::optional<CompatibilityInfo> compatibility;
        std::vector<std::string> dependencies;
        
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PluginInfo, id, name, description, version, 
                                                   author, tags, image, scenes, releases, 
                                                   compatibility, dependencies)
    };

    struct MarketplaceIndex {
        std::string version;
        std::vector<PluginInfo> plugins;
        
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(MarketplaceIndex, version, plugins)
    };

    enum class InstallationStatus {
        NOT_INSTALLED,
        INSTALLED,
        UPDATE_AVAILABLE,
        DOWNLOADING,
        INSTALLING,
        ERROR
    };

    struct InstalledPlugin {
        std::string id;
        std::string version;
        std::string install_path;
        bool enabled;
        
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(InstalledPlugin, id, version, install_path, enabled)
    };

    struct InstallationProgress {
        std::string plugin_id;
        InstallationStatus status;
        double progress; // 0.0 to 1.0
        std::optional<std::string> error_message;
        
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(InstallationProgress, plugin_id, status, progress, error_message)
    };

} // namespace Marketplace
} // namespace Plugins