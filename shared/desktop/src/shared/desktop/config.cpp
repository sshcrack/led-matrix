#include "shared/desktop/config.h"
#include "shared/desktop/plugin_loader/loader.h"
#include <spdlog/spdlog.h>
#include <shared/common/utils/utils.h>
#include <iostream>
#include <fstream>
#include <mutex>

// General class special member functions
Config::General::General(const General &other)
{
    std::shared_lock<std::shared_mutex> lock(other.mutex_);
    hostname = other.hostname;
    autostart = other.autostart;
}

Config::General &Config::General::operator=(const General &other)
{
    if (this != &other)
    {
        std::unique_lock<std::shared_mutex> lock1(mutex_, std::defer_lock);
        std::shared_lock<std::shared_mutex> lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);

        hostname = other.hostname;
        autostart = other.autostart;
    }
    return *this;
}

Config::General::General(General &&other) noexcept
{
    std::unique_lock<std::shared_mutex> lock(other.mutex_);
    hostname = std::move(other.hostname);
    autostart = other.autostart;
}

Config::General &Config::General::operator=(General &&other) noexcept
{
    if (this != &other)
    {
        std::unique_lock<std::shared_mutex> lock1(mutex_, std::defer_lock);
        std::unique_lock<std::shared_mutex> lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);

        hostname = std::move(other.hostname);
        autostart = other.autostart;
    }
    return *this;
}

bool Config::General::isAutostartEnabled() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
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
        std::unique_lock<std::shared_mutex> lock(mutex_);
        autostart = enabled;
        spdlog::info("Autostart set to {}", enabled);
    }
}

const std::string &Config::General::getHostname() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return hostname;
}

std::string Config::General::getHostnameCopy() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return hostname;
}

void Config::General::setHostnameAndPort(const std::string &hostname)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
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
    saveConfig();
}

void Config::ConfigManager::saveConfig()
{
    json configJson;
    json pluginSettingsCopy;

    // Create a copy of the data under lock
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        to_json(configJson["general"], generalConfig);
        pluginSettingsCopy = pluginSettings;

        // Update plugin settings from plugins
        for (auto plugin : Plugins::PluginManager::instance()->get_plugins())
        {
            json j;
            plugin.second->saveConfig(j);
            pluginSettingsCopy[plugin.first] = j;
        }
    }

    configJson["pluginSettings"] = pluginSettingsCopy;

    std::ofstream configFile(configFilePath);
    if (!configFile.is_open())
    {
        spdlog::error("Failed to open configuration file for writing: {}", configFilePath.string());
        return;
    }

    configFile << configJson.dump(4); // Pretty print with 4 spaces
    spdlog::info("Configuration saved to {}", configFilePath.string());
    configFile.close();

    // Update the internal plugin settings with the new data
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        pluginSettings = std::move(pluginSettingsCopy);
    }
}

Config::ConfigManager *Config::ConfigManager::instance()
{
    static std::once_flag onceFlag;
    static Config::ConfigManager *_instance = nullptr;

    std::call_once(onceFlag, []()
                   {
        std::filesystem::path configFilePath = get_exec_dir() / "config.json";
        _instance = new Config::ConfigManager(configFilePath); });

    return _instance;
}