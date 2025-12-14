#include "shared/matrix/plugin_loader/PluginStore.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"
#include <cpr/cpr.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>

using namespace spdlog;
namespace fs = std::filesystem;

namespace Plugins {

    PluginStore& PluginStore::instance() {
        static PluginStore instance;
        return instance;
    }

    std::string PluginStore::get_github_api_url(const std::string& owner, const std::string& repo) {
        return "https://api.github.com/repos/" + owner + "/" + repo + "/releases";
    }

    std::expected<std::vector<PluginRelease>, std::string> PluginStore::get_releases(const std::string& owner, const std::string& repo) {
        try {
            std::string api_url = get_github_api_url(owner, repo);
            debug("Fetching releases from: {}", api_url);

            auto response = cpr::Get(
                cpr::Url{api_url},
                cpr::Header{{"User-Agent", "led-matrix-plugin-store"}}
            );

            if (response.status_code != 200) {
                return std::unexpected("GitHub API request failed with status: " + std::to_string(response.status_code));
            }

            auto json_response = nlohmann::json::parse(response.text);
            std::vector<PluginRelease> releases;

            if (!json_response.is_array()) {
                 return std::unexpected("GitHub API returned invalid format (expected array)");
            }

            for (const auto& release : json_response) {
                if (release["draft"].get<bool>()) continue;

                std::string tag_name = release["tag_name"].get<std::string>();
                
                // Parse tag format: PluginName-vX.Y.Z
                // Find the last occurrence of "-v" to split plugin name and version
                size_t version_pos = tag_name.rfind("-v");
                if (version_pos == std::string::npos) {
                    debug("Skipping release with invalid tag format: {}", tag_name);
                    continue; // Skip releases that don't match our format
                }
                
                std::string plugin_name = tag_name.substr(0, version_pos);
                std::string version = tag_name.substr(version_pos + 2); // Skip "-v"
                
                if (plugin_name.empty() || version.empty()) {
                    debug("Skipping release with empty plugin name or version: {}", tag_name);
                    continue;
                }
                
                PluginRelease info;
                info.tag_name = tag_name;
                info.name = plugin_name;
                info.version = version;
                info.body = release.value("body", "");
                info.prerelease = release.value("prerelease", false);
                info.published_at = release["published_at"].get<std::string>();
                
                // Find the matching asset (should be PluginName.zip or similar)
                bool found_asset = false;
                if (release.contains("assets")) {
                    for (const auto& asset : release["assets"]) {
                        std::string asset_name = asset["name"].get<std::string>();
                        
                        // Look for assets that start with plugin name
                        if (asset_name.ends_with(".zip") || asset_name.ends_with(".tar.gz") || asset_name.ends_with(".so")) {
                            // Extract stem from asset name
                            std::string stem = fs::path(asset_name).stem().string();
                            if (asset_name.ends_with(".tar.gz")) {
                                stem = fs::path(stem).stem().string();
                            }
                            
                            // Check if asset matches plugin name
                            if (stem == plugin_name) {
                                info.download_url = asset["browser_download_url"].get<std::string>();
                                info.size = asset["size"].get<size_t>();
                                found_asset = true;
                                break;
                            }
                        }
                    }
                }
                
                if (found_asset) {
                    releases.push_back(info);
                } else {
                    debug("No matching asset found for plugin: {}", plugin_name);
                }
            }

            return releases;
        }
        catch (const std::exception& ex) {
            return std::unexpected("Error fetching releases: " + std::string(ex.what()));
        }
    }

    std::expected<void, std::string> PluginStore::install_plugin(const std::string& url, const std::string& filename) {
        try {
            auto exec_dir = get_exec_dir();
            fs::path plugin_dir = exec_dir / "plugins";
            if (!fs::exists(plugin_dir)) {
                fs::create_directories(plugin_dir);
            }

            fs::path temp_file = fs::temp_directory_path() / filename;
            if (fs::exists(temp_file)) fs::remove(temp_file);

            info("Downloading plugin from: {}", url);
            std::ofstream file(temp_file, std::ios::binary);
            if (!file) {
                 return std::unexpected("Could not create temp file: " + temp_file.string());
            }

            auto response = cpr::Download(file, cpr::Url{url});
            file.close();

            if (response.status_code != 200) {
                return std::unexpected("Download failed with status: " + std::to_string(response.status_code));
            }

            // Install logic
            // 1. Determine type
            std::string ext = temp_file.extension().string();
            std::string plugin_name = fs::path(filename).stem().string();
            // Handle double extension like .tar.gz
            if (filename.ends_with(".tar.gz")) {
                plugin_name = fs::path(plugin_name).stem().string(); // remove .tar
                ext = ".tar.gz";
            }

            fs::path target_dir = plugin_dir / plugin_name;
            if (fs::exists(target_dir)) {
                // simple upgrade: remove old dir
                // TODO: backup?
                fs::remove_all(target_dir);
            }
            fs::create_directories(target_dir);

            if (ext == ".zip") {
                std::string cmd = "unzip -o -q \"" + temp_file.string() + "\" -d \"" + target_dir.string() + "\"";
                int ret = std::system(cmd.c_str());
                if (ret != 0) return std::unexpected("Unzip failed");
            } else if (ext == ".tar.gz" || ext == ".tgz") {
                 std::string cmd = "tar -xzf \"" + temp_file.string() + "\" -C \"" + target_dir.string() + "\"";
                 int ret = std::system(cmd.c_str());
                 if (ret != 0) return std::unexpected("Untar failed");
            } else if (ext == ".so") {
                // If it's a direct .so, move it to target_dir/libNAME.so
                // Loader expects plugins/NAME/libNAME.so
                fs::path target_file = target_dir / ("lib" + plugin_name + ".so");
                fs::copy_file(temp_file, target_file, fs::copy_options::overwrite_existing);
            } else {
                 return std::unexpected("Unsupported file extension: " + ext);
            }

            fs::remove(temp_file);
            info("Plugin installed to {}", target_dir.string());
            
            return {};
        } catch (const std::exception& ex) {
            return std::unexpected("Error installing plugin: " + std::string(ex.what()));
        }
    }

    std::expected<void, std::string> PluginStore::remove_plugin(const std::string& name) {
        try {
             auto exec_dir = get_exec_dir();
             fs::path plugin_dir = exec_dir / "plugins" / name;
             if (fs::exists(plugin_dir)) {
                 fs::remove_all(plugin_dir);
                 return {};
             } else {
                 return std::unexpected("Plugin not found");
             }
        } catch (const std::exception& ex) {
            return std::unexpected("Error removing plugin: " + std::string(ex.what()));
        }
    }

}
