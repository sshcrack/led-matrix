#include "shared/config/data.h"
#include "shared/plugin_loader/loader.h"
#include <nlohmann/json.hpp>
#include <random>
#include <spdlog/spdlog.h>
#include <chrono>
#include <ctime>

using namespace std;
using namespace spdlog;
using json = nlohmann::json;

namespace ConfigData {
    void to_json(json &j, const Scenes::Scene *&p) {
        auto &c = const_cast<Scenes::Scene *&>(p);

        j = {
            {"type", c->get_name()},
            {"arguments", c->to_json()},
            {"uuid", c->get_uuid()}
        };
    }

    void to_json(json &j, const ImageProviders::General *&p) {
        auto &c = const_cast<ImageProviders::General *&>(p);

        j = {
            {"type", c->get_name()},
            {"arguments", c->to_json()}
        };
    }


    void to_json(json &j, const SpotifyData &p) {
        j = json{
            {"expires_at", p.expires_at},
            {"access_token", p.access_token.value_or("")},
            {"refresh_token", p.refresh_token.value_or("")}
        };
    }

    void to_json(json &j, std::shared_ptr<Preset> p) {
        vector<json> scenes_json;
        scenes_json.reserve(p->scenes.size());
        for (const auto &item: p->scenes) {
            json local_j;
            to_json(local_j, (const Scenes::Scene *&) item);

            scenes_json.push_back(local_j);
        }

        j = json{
            {"scenes", scenes_json}
        };
    }

    void to_json(json &j, const Root &p) {
        j = json{
            {"presets", p.presets},
            {"curr", p.curr},
            {"spotify", p.spotify},
            {"pluginConfigs", p.pluginConfigs},
            {"schedules", p.schedules},
            {"scheduling_enabled", p.scheduling_enabled}
        };
    }

    void from_json(const json &j, Root &p) {
        p.curr = j.value("curr", "Default");
        if (j.contains("presets")) {
            j.at("presets").get_to(p.presets);
        } else {
            p.presets = {
                {"Default", Preset::create_default()}
            };
        }

        p.spotify = j.value("spotify", SpotifyData());
        p.pluginConfigs = j.value("pluginConfigs", std::map<string, string>());
        p.schedules = j.value("schedules", std::map<string, Schedule>());
        p.scheduling_enabled = j.value("scheduling_enabled", false);
    }

    void from_json(const json &j, Schedule &p) {
        j.at("id").get_to(p.id);
        j.at("name").get_to(p.name);
        j.at("preset_id").get_to(p.preset_id);
        j.at("start_hour").get_to(p.start_hour);
        j.at("start_minute").get_to(p.start_minute);
        j.at("end_hour").get_to(p.end_hour);
        j.at("end_minute").get_to(p.end_minute);
        j.at("days_of_week").get_to(p.days_of_week);
        p.enabled = j.value("enabled", true);
    }

    void from_json(const json &j, SpotifyData &p) {
        string access, refresh;
        tmillis_t expires_at;

        if (j.contains("access_token"))
            j.at("access_token").get_to(access);

        if (j.contains("refresh_token"))
            j.at("refresh_token").get_to(refresh);

        if (j.contains("expires_at"))
            j.at("expires_at").get_to(expires_at);

        if (!access.empty()) {
            p.access_token = access;
        }

        if (!refresh.empty()) {
            p.refresh_token = refresh;
        }

        p.expires_at = expires_at;
    }

    void from_json(const json &j, std::shared_ptr<Preset> &p) {
        vector<std::shared_ptr<Scenes::Scene> > scenes;
        if (j.contains("scenes")) {
            vector<json> scenes_json = j.at("scenes");

            scenes.reserve(scenes_json.size());

            for (const auto &item: scenes_json)
                scenes.push_back(Scenes::Scene::from_json(item));
        } else {
            info("No scenes in preset. Adding default...");
            p = Preset::create_default();
        }


        p = {
            new Preset(),
            [](Preset *p) {
                delete p;
            }
        };

        p->scenes = std::move(scenes);
    }

    void from_json(const json &j, std::unique_ptr<ImageProviders::General, void(*)(ImageProviders::General *)> &p) {
        p = std::move(ImageProviders::General::from_json(j));
    }

    void from_json(const json &j, std::unique_ptr<Scenes::Scene, void(*)(Scenes::Scene *)> &p) {
        p = std::move(Scenes::Scene::from_json(j));
    }

    bool SpotifyData::has_auth() const {
        return !(this->access_token->empty() || this->refresh_token->empty() || this->expires_at == 0);
    }

    std::shared_ptr<Preset> Preset::create_default() {
        vector<shared_ptr<Scenes::Scene> > scenes;
        for (const auto &scene_wrapper: Plugins::PluginManager::instance()->get_scenes()) {
            auto scene = scene_wrapper->create();
            
            scene->update_default_properties();
            scene->register_properties();

            // Pass no json to default initialize
            spdlog::debug("Creating default scene: {}", scene->get_name());
            scene->load_properties(nlohmann::json::object());

            scenes.push_back(std::move(scene));
        }

        auto preset = new Preset();
        preset->scenes = scenes;

        return {
            preset,
            [](Preset *p) {
                delete p;
            }
        };
    }

    bool SpotifyData::is_expired() const {
        return GetTimeInMillis() > this->expires_at;
    }

    bool Schedule::is_active_now() const {
        if (!enabled) return false;
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto* tm = std::localtime(&time_t);
        
        return is_active_at_time(tm->tm_hour, tm->tm_min, tm->tm_wday);
    }
    
    bool Schedule::is_active_at_time(int hour, int minute, int day_of_week) const {
        if (!enabled) return false;
        
        // Check if today is in the scheduled days
        if (std::find(days_of_week.begin(), days_of_week.end(), day_of_week) == days_of_week.end()) {
            return false;
        }
        
        // Convert times to minutes for easier comparison
        int current_minutes = hour * 60 + minute;
        int start_minutes = start_hour * 60 + start_minute;
        int end_minutes = end_hour * 60 + end_minute;
        
        // Handle schedules that cross midnight
        if (start_minutes > end_minutes) {
            return current_minutes >= start_minutes || current_minutes <= end_minutes;
        } else {
            return current_minutes >= start_minutes && current_minutes <= end_minutes;
        }
    }

    void to_json(json &j, const Schedule &p) {
        j = json{
            {"id", p.id},
            {"name", p.name},
            {"preset_id", p.preset_id},
            {"start_hour", p.start_hour},
            {"start_minute", p.start_minute},
            {"end_hour", p.end_hour},
            {"end_minute", p.end_minute},
            {"days_of_week", p.days_of_week},
            {"enabled", p.enabled}
        };
    }
}
