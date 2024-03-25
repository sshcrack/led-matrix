//
// Created by hendrik on 2/2/24.
//

#include "data.h"
#include <nlohmann/json.hpp>
#include <random>
#include <spdlog/spdlog.h>

using namespace std;
using json = nlohmann::json;

namespace ConfigData {
    void to_json(json& j, const ImageTypes::General*& p)  {
        spdlog::debug("To json imgtype", to_string(j));
        auto& c = const_cast<ImageTypes::General *&>(p);
        j = c->to_json();
    }


    void to_json(json& j, const SpotifyData& p)  {
        j = json{
                {"expires_at", p.expires_at},
                {"access_token", p.access_token.value_or("")},
                {"refresh_token", p.refresh_token.value_or("")}
        };
    }

    void to_json(json& j, const Preset& p)  {
        spdlog::debug("To json group", to_string(j));
        vector<json> image_json;

        image_json.reserve(p.categories.size());
        for (const auto &item: p.categories)
            image_json.push_back(item->to_json());

        j = json{
            {"name", p.name},
            {"images", image_json}
        };
    }

    void to_json(json& j, const Root& p) {
        spdlog::debug("To json root", to_string(j));

        j = json{
            {"presets", p.presets},
            {"curr", p.curr},
            {"spotify", p.spotify}
        };
    }

    void from_json(const json& j, Root& p) {
        spdlog::debug("from json root", to_string(j));
        j.at("curr").get_to(p.curr);
        j.at("presets").get_to(p.presets);
        j.at("spotify").get_to(p.spotify);
    }

    void from_json(const json& j, SpotifyData& p) {
        string access, refresh;
        tmillis_t expires_at;
        j.at("access_token").get_to(access);
        j.at("refresh_token").get_to(refresh);
        j.at("expires_at").get_to(expires_at);

        if(!access.empty()) {
            p.access_token = access;
        }

        if(!refresh.empty()) {
            p.refresh_token = refresh;
        }

        p.expires_at = expires_at;
    }

    void from_json(const json& j, Preset& p) {
        spdlog::debug("from json preset {}", to_string(j));
        j.at("name").get_to(p.name);

        spdlog::debug("From image get");
        vector<json> image_json = j.at("images");

        vector<ImageTypes::General*> images;
        images.reserve(image_json.size());

        spdlog::debug("array");
        for (const auto &item: image_json)
            images.push_back(ImageTypes::General::from_json(item));

        spdlog::debug("set");
        p.categories = images;
    }

    void from_json(const json& j, ImageTypes::General*& p) {
        spdlog::debug("from json imgtype", to_string(j));
        p = ImageTypes::General::from_json(j);
    }

    void Preset::randomize() {
        std::shuffle(this->categories.begin(), this->categories.end(), std::random_device());
    }
}