#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <shared/common/utils/utils.h>
#include <autostart.h>

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
        void setHostname(const std::string &hostname);
    };

    void to_json(json &j, const General &p);
    void from_json(const json &j, General &p);

    class ConfigManager
    {
        private:
            General generalConfig;
            json pluginSettings;
            const std::string configFilePath;

        public:
            static ConfigManager &instance()
            {
                static ConfigManager instance(get_exec_file().parent_path() / "config.json");
                return instance;
            }

            ConfigManager(const std::string &filePath);
            ~ConfigManager();

            General &getGeneralConfig() {
                return generalConfig;
            }

            const json &getPluginSettings() const {
                return pluginSettings;
            }

            void setPluginSetting(const std::string name, const json &settings) {
                pluginSettings[name] = settings;
            }

            void saveConfig(const std::string &filePath) const;

    };
}