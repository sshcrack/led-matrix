#include "shared/desktop/config.h"
#include "shared/desktop/plugin_loader/loader.h"
#include <spdlog/spdlog.h>
#include <shared/common/utils/utils.h>
#include <iostream>
#include <fstream>

bool Config::General::isAutostartEnabled() const
{
    return autostart;
}

void Config::General::setAutostartEnabled(bool enabled)
{
    auto res = enabled ? Autostart::enable(get_exec_file().string(), APP_NAME) : Autostart::disable(APP_NAME);

    if (!res)
    {
        spdlog::error("Failed to set autostart: {}", res.error());
    }
    else
    {
        autostart = enabled;
        spdlog::info("Autostart set to {}", enabled);
    }
}

const std::string &Config::General::getHostname() const
{
    return hostname;
}

void Config::General::setHostnameAndPort(const std::string &hostname)
{
    this->hostname = hostname;
}

void Config::from_json(const json &j, General &p)
{
    if (j.contains("hostname"))
    {
        std::string hostname;
        j.at("hostname").get_to(hostname);
        p.setHostnameAndPort(hostname);
    }
}

void Config::to_json(json &j, const General &p)
{
    j = {
        {"hostname", p.getHostname()}};
}

Config::ConfigManager::ConfigManager(const std::filesystem::path &filePath)
{
    configFilePath = filePath;
    std::ifstream configFile(filePath);
    if (configFile.is_open())
    {
        try
        {
            nlohmann::json configJson = json::parse(configFile);
            spdlog::info("Configuration loaded from {}", filePath.string());
            generalConfig = configJson.value("general", json::object());

            if (configJson.contains("pluginSettings"))
            {
                pluginSettings = configJson.value("pluginSettings", json::object());
                spdlog::info("Plugin settings loaded from configuration file.");
            }
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to parse configuration file: {}", e.what());
        }

        configFile.close();
    }
    else
    {
        spdlog::warn("Configuration file at {} not found, using default settings.", filePath.string());
    }
}

Config::ConfigManager::~ConfigManager()
{
    saveConfig(configFilePath);
}

void Config::ConfigManager::saveConfig(const std::filesystem::path &filePath)
{
    for(auto plugin : Plugins::PluginManager::instance()->get_plugins()) {
        json j;
        plugin.second->saveConfig(j);

        pluginSettings[plugin.first] = j;
    };

    json configJson;
    configJson["general"] = this->generalConfig;
    configJson["pluginSettings"] = pluginSettings;

    std::ofstream configFile(filePath);
    if (configFile.is_open())
    {
        configFile << configJson.dump(4); // Pretty print with 4 spaces
        spdlog::info("Configuration saved to {}", filePath.string());
        configFile.close();
    }
    else
    {
        spdlog::error("Failed to open configuration file for writing: {}", filePath.string());
    }
}

Config::ConfigManager *_instance = nullptr;

Config::ConfigManager *Config::ConfigManager::instance()
{
    if (_instance == nullptr)
    {
        std::filesystem::path configFilePath = get_exec_dir() / "config.json";
        _instance = new Config::ConfigManager(configFilePath);
    }

    return _instance;
}