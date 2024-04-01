
#include "shared/config/data.h"
#include "shared/plugin_loader/loader.h"
#include <nlohmann/json.hpp>
#include <random>
#include <spdlog/spdlog.h>

using namespace std;
using json = nlohmann::json;

namespace ConfigData {
    void to_json(json &j, const ImageProviders::General *&p) {
        auto &c = const_cast<ImageProviders::General *&>(p);
        auto pl = Plugins::PluginManager::instance();

        auto type_str = pl->get_name_of_provider(c);
        if (!type_str.has_value()) {
            spdlog::error("Could not get name of provider");
            throw runtime_error(fmt::format("Could not get name of provider: {}", c->to_json().dump()));
        }

        j = {
                {"type",      type_str.value()},
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
        for (const auto &item: p.providers)
            image_json.push_back(item->to_json());

        vector<json> scenes_json;
        scenes_json.reserve(p.scenes.size());
        for (const auto &item: p.scenes)
            scenes_json.push_back(item->to_json());

        j = json{
                {"name",   p.name},
                {"images", image_json},
                {"scenes", }
        };
    }

    void to_json(json &j, const Root &p) {
        j = json{
                {"presets", p.presets},
                {"curr",    p.curr},
                {"spotify", p.spotify}
        };
    }

    void from_json(const json &j, Root &p) {
        spdlog::debug("from json root", to_string(j));
        j.at("curr").get_to(p.curr);
        j.at("presets").get_to(p.presets);
        j.at("spotify").get_to(p.spotify);
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
        spdlog::debug("from json preset {}", to_string(j));
        j.at("name").get_to(p.name);

        vector<json> image_json = j.at("images");

        vector<ImageProviders::General *> images;
        images.reserve(image_json.size());

        for (const auto &item: image_json)
            images.push_back(ImageProviders::General::from_json(item));

        p.providers = images;
    }

    void from_json(const json &j, ImageProviders::General *&p) {
        spdlog::debug("from json imgtype", to_string(j));
        p = ImageProviders::General::from_json(j);
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