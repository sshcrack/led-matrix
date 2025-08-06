#include "shared/common/marketplace/types.h"

namespace Plugins {
namespace Marketplace {

    void to_json(nlohmann::json& j, const SceneInfo& s) {
        j = nlohmann::json{
            {"name", s.name},
            {"description", s.description}
        };
    }

    void from_json(const nlohmann::json& j, SceneInfo& s) {
        j.at("name").get_to(s.name);
        j.at("description").get_to(s.description);
    }

    void to_json(nlohmann::json& j, const BinaryInfo& b) {
        j = nlohmann::json{
            {"url", b.url},
            {"sha512", b.sha512},
            {"size", b.size}
        };
    }

    void from_json(const nlohmann::json& j, BinaryInfo& b) {
        j.at("url").get_to(b.url);
        j.at("sha512").get_to(b.sha512);
        j.at("size").get_to(b.size);
    }

    void to_json(nlohmann::json& j, const ReleaseInfo& r) {
        j = nlohmann::json{};
        if (r.matrix.has_value()) {
            j["matrix"] = r.matrix.value();
        }
        if (r.desktop.has_value()) {
            j["desktop"] = r.desktop.value();
        }
    }

    void from_json(const nlohmann::json& j, ReleaseInfo& r) {
        if (j.contains("matrix")) {
            r.matrix = j["matrix"].get<BinaryInfo>();
        }
        if (j.contains("desktop")) {
            r.desktop = j["desktop"].get<BinaryInfo>();
        }
    }

    void to_json(nlohmann::json& j, const CompatibilityInfo& c) {
        j = nlohmann::json{};
        if (c.matrix_version.has_value()) {
            j["matrix_version"] = c.matrix_version.value();
        }
        if (c.desktop_version.has_value()) {
            j["desktop_version"] = c.desktop_version.value();
        }
    }

    void from_json(const nlohmann::json& j, CompatibilityInfo& c) {
        if (j.contains("matrix_version")) {
            c.matrix_version = j["matrix_version"].get<std::string>();
        }
        if (j.contains("desktop_version")) {
            c.desktop_version = j["desktop_version"].get<std::string>();
        }
    }

    void to_json(nlohmann::json& j, const PluginInfo& p) {
        j = nlohmann::json{
            {"id", p.id},
            {"name", p.name},
            {"description", p.description},
            {"version", p.version},
            {"author", p.author},
            {"tags", p.tags},
            {"scenes", p.scenes},
            {"releases", p.releases},
            {"dependencies", p.dependencies}
        };
        
        if (p.image.has_value()) {
            j["image"] = p.image.value();
        }
        if (p.compatibility.has_value()) {
            j["compatibility"] = p.compatibility.value();
        }
    }

    void from_json(const nlohmann::json& j, PluginInfo& p) {
        j.at("id").get_to(p.id);
        j.at("name").get_to(p.name);
        j.at("description").get_to(p.description);
        j.at("version").get_to(p.version);
        j.at("author").get_to(p.author);
        j.at("tags").get_to(p.tags);
        j.at("scenes").get_to(p.scenes);
        j.at("releases").get_to(p.releases);
        j.at("dependencies").get_to(p.dependencies);
        
        if (j.contains("image")) {
            p.image = j["image"].get<std::string>();
        }
        if (j.contains("compatibility")) {
            p.compatibility = j["compatibility"].get<CompatibilityInfo>();
        }
    }

    void to_json(nlohmann::json& j, const MarketplaceIndex& m) {
        j = nlohmann::json{
            {"version", m.version},
            {"plugins", m.plugins}
        };
    }

    void from_json(const nlohmann::json& j, MarketplaceIndex& m) {
        j.at("version").get_to(m.version);
        j.at("plugins").get_to(m.plugins);
    }

    void to_json(nlohmann::json& j, const InstalledPlugin& p) {
        j = nlohmann::json{
            {"id", p.id},
            {"version", p.version},
            {"install_path", p.install_path},
            {"enabled", p.enabled}
        };
    }

    void from_json(const nlohmann::json& j, InstalledPlugin& p) {
        j.at("id").get_to(p.id);
        j.at("version").get_to(p.version);
        j.at("install_path").get_to(p.install_path);
        j.at("enabled").get_to(p.enabled);
    }

    void to_json(nlohmann::json& j, const InstallationProgress& p) {
        j = nlohmann::json{
            {"plugin_id", p.plugin_id},
            {"status", static_cast<int>(p.status)},
            {"progress", p.progress}
        };
        
        if (p.error_message.has_value()) {
            j["error_message"] = p.error_message.value();
        }
    }

    void from_json(const nlohmann::json& j, InstallationProgress& p) {
        j.at("plugin_id").get_to(p.plugin_id);
        p.status = static_cast<InstallationStatus>(j.at("status").get<int>());
        j.at("progress").get_to(p.progress);
        
        if (j.contains("error_message")) {
            p.error_message = j["error_message"].get<std::string>();
        }
    }

} // namespace Marketplace
} // namespace Plugins