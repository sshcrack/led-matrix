
#include "shared/config/data.h"
#include "shared/plugin_loader/loader.h"
#include <nlohmann/json.hpp>
#include <random>
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;
using json = nlohmann::json;

namespace ConfigData {
    void to_json(json &j, const Scenes::Scene *&p) {
        auto &c = const_cast<Scenes::Scene *&>(p);

        j = {
                {"type",      c->get_name()},
                {"arguments", c->to_json()}
        };
    }

    void to_json(json &j, const ImageProviders::General *&p) {
        auto &c = const_cast<ImageProviders::General *&>(p);

        j = {
                {"type",      c->get_name()},
                {"arguments", c->to_json()}
        };
    }


    void to_json(json &j, const SpotifyData &p) {
        j = json{
                {"expires_at",    p.expires_at},
                {"access_token",  p.access_token.value_or("")},
                {"refresh_token", p.refresh_token.value_or("")}
        };
    }

    void to_json(json &j, const Preset &p) {
        vector<json> image_json;

        image_json.reserve(p.providers.size());
        for (const auto &item: p.providers) {
            json local_j;
            to_json(local_j, (const ImageProviders::General *&) item);

            image_json.push_back(local_j);
        }

        vector<json> scenes_json;
        scenes_json.reserve(p.scenes.size());
        for (const auto &item: p.scenes) {
            json local_j;
            to_json(local_j, (const Scenes::Scene *&) item);

            scenes_json.push_back(local_j);
        }

        j = json{
                {"name",   p.name},
                {"images", image_json},
                {"scenes", scenes_json}
        };
    }

    void to_json(json &j, const Root &p) {
        j = json{
                {"presets", p.presets},
                {"curr",    p.curr},
                {"spotify", p.spotify},
                {"pluginConfigs", p.pluginConfigs}
        };
    }

    void from_json(const json &j, Root &p) {
        j.at("curr").get_to(p.curr);
        j.at("presets").get_to(p.presets);
        j.at("spotify").get_to(p.spotify);
        j.at("pluginConfigs").get_to(p.pluginConfigs);
    }

    void from_json(const json &j, SpotifyData &p) {
        string access, refresh;
        tmillis_t expires_at;
        j.at("access_token").get_to(access);
        j.at("refresh_token").get_to(refresh);
        j.at("expires_at").get_to(expires_at);

        if (!access.empty()) {
            p.access_token = access;
        }

        if (!refresh.empty()) {
            p.refresh_token = refresh;
        }

        p.expires_at = expires_at;
    }

    void from_json(const json &j, Preset &p) {
        j.at("name").get_to(p.name);

        vector<json> image_json = j.at("images");


        vector<ImageProviders::General *> images;
        images.reserve(image_json.size());

        for (const auto &item: image_json)
            images.push_back(ImageProviders::General::from_json(item));


        vector<Scenes::Scene *> scenes;
        if (j.contains("scenes")) {
            vector<json> scenes_json = j.at("scenes");

            scenes.reserve(scenes_json.size());

            for (const auto &item: scenes_json)
                scenes.push_back(Scenes::Scene::from_json(item));

        } else {
            spdlog::info("No scenes in preset. Adding default...");
            auto pl = Plugins::PluginManager::instance();
            for (const auto &item: pl->get_scenes()) {
                scenes.emplace_back(item->create_default());
            }
        }

        p.scenes = scenes;
        p.providers = images;
    }

    void from_json(const json &j, ImageProviders::General *&p) {
        p = ImageProviders::General::from_json(j);
    }

    void from_json(const json &j, Scenes::Scene *&p) {
        p = Scenes::Scene::from_json(j);
    }

    void Preset::randomize() {
        std::shuffle(this->providers.begin(), this->providers.end(), std::random_device());
    }

    bool SpotifyData::has_auth() const {
        return !(this->access_token->empty() || this->refresh_token->empty() || this->expires_at == 0);
    }

    bool SpotifyData::is_expired() const {
        return GetTimeInMillis() > this->expires_at;
    }
}