#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <shared/common/utils/utils.h>
#include "autostart.h"

using json = nlohmann::json;
static const char *APP_NAME = "LED_Matrix_Controller";
namespace Config
{
    class General
    {
    protected:
        std::string hostname;
        bool autostart = Autostart::isEnabled(APP_NAME);

    public:

        bool isAutostartEnabled() const;
        void setAutostartEnabled(bool enabled);

        const std::string &getHostname() const;
        void setHostnameAndPort(const std::string &hostname);
    };

    void to_json(json &j, const General &p);
    void from_json(const json &j, General &p);

    class ConfigManager
    {
        private:
            General generalConfig;
            json pluginSettings = json::object();
            std::filesystem::path configFilePath;

        public:
            static ConfigManager *instance();

            ConfigManager(const std::filesystem::path &filePath);
            ~ConfigManager();

            General &getGeneralConfig() {
                return generalConfig;
            }

            const json &getPluginSetting(const std::string pluginName) const {
                return pluginSettings[pluginName];
            }

            void setPluginSetting(const std::string pluginName, const json &settings) {
                pluginSettings[pluginName] = settings;
            }

            void saveConfig(const std::filesystem::path &filePath) const;

    };
}