#pragma once
#include <string>
#include <mutex>
#include <shared_mutex>
#include <nlohmann/json.hpp>
#include <shared/common/utils/utils.h>
#include "autostart.h"

using json = nlohmann::json;
static const char *APP_NAME = "LED_Matrix_Controller";
namespace Config
{
    /**
     * @brief Thread-safe general configuration class
     *
     * All methods in this class are thread-safe and can be called from multiple threads.
     * Uses a shared_mutex to allow concurrent reads while ensuring exclusive writes.
     */
    class General
    {
    protected:
        std::string hostname;
        uint16_t port = 8080; // Default port

        bool autostart = Autostart::isEnabled(APP_NAME);
        mutable std::shared_mutex mutex_;

    public:
        General() = default;
        General(const General &other);
        General &operator=(const General &other);
        General(General &&other) noexcept;
        General &operator=(General &&other) noexcept;

        bool isAutostartEnabled() const;
        void setAutostartEnabled(bool enabled);

        const uint16_t &getPort() const;
        void setPort(uint16_t newPort);

        const std::string &getHostname() const;
        std::string getHostnameCopy() const; // Thread-safe copy method
        void setHostname(const std::string &hostname);
    };

    void to_json(json &j, const General &p);
    void from_json(const json &j, General &p);

    /**
     * @brief Thread-safe configuration manager with singleton pattern
     *
     * This class manages the application configuration and provides thread-safe
     * access to general settings and plugin-specific settings. Uses a shared_mutex
     * to allow concurrent reads while ensuring exclusive writes.
     */
    class ConfigManager
    {
    private:
        General generalConfig;
        json pluginSettings = json::object();
        std::filesystem::path configFilePath;
        mutable std::shared_mutex mutex_;

    public:
        static ConfigManager *instance();

        ConfigManager(const std::filesystem::path &filePath);
        ~ConfigManager();

        General &getGeneralConfig()
        {
            // Note: This returns a reference to the general config.
            // Callers should be aware that the General class itself is thread-safe
            // through its internal mutex, but direct access to members should use
            // the General class's getter/setter methods.
            return generalConfig;
        }

        std::optional<const nlohmann::json> getPluginSetting(const std::string pluginName) const
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            if (!pluginSettings.contains(pluginName))
                return std::nullopt;

            return pluginSettings.at(pluginName);
        }

        void setPluginSetting(const std::string pluginName, const json &settings)
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            pluginSettings[pluginName] = settings;
        }

        void saveConfig();
    };
}