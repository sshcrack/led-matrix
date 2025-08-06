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
    };

    struct BinaryInfo {
        std::string url;
        std::string sha512;
        size_t size;
    };

    struct ReleaseInfo {
        std::optional<BinaryInfo> matrix;
        std::optional<BinaryInfo> desktop;
    };

    struct CompatibilityInfo {
        std::optional<std::string> matrix_version;
        std::optional<std::string> desktop_version;
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
    };

    struct MarketplaceIndex {
        std::string version;
        std::vector<PluginInfo> plugins;
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
    };

    struct InstallationProgress {
        std::string plugin_id;
        InstallationStatus status;
        double progress; // 0.0 to 1.0
        std::optional<std::string> error_message;
    };

    // JSON serialization functions
    void to_json(nlohmann::json& j, const SceneInfo& s);
    void from_json(const nlohmann::json& j, SceneInfo& s);
    
    void to_json(nlohmann::json& j, const BinaryInfo& b);
    void from_json(const nlohmann::json& j, BinaryInfo& b);
    
    void to_json(nlohmann::json& j, const ReleaseInfo& r);
    void from_json(const nlohmann::json& j, ReleaseInfo& r);
    
    void to_json(nlohmann::json& j, const CompatibilityInfo& c);
    void from_json(const nlohmann::json& j, CompatibilityInfo& c);
    
    void to_json(nlohmann::json& j, const PluginInfo& p);
    void from_json(const nlohmann::json& j, PluginInfo& p);
    
    void to_json(nlohmann::json& j, const MarketplaceIndex& m);
    void from_json(const nlohmann::json& j, MarketplaceIndex& m);
    
    void to_json(nlohmann::json& j, const InstalledPlugin& p);
    void from_json(const nlohmann::json& j, InstalledPlugin& p);
    
    void to_json(nlohmann::json& j, const InstallationProgress& p);
    void from_json(const nlohmann::json& j, InstallationProgress& p);

} // namespace Marketplace
} // namespace Plugins