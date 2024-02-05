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

    void to_json(json& j, const Preset& p)  {
        spdlog::debug("To json group", to_string(j));
        vector<json> image_json;

        image_json.reserve(p.categories.size());
        for (const auto &item: p.categories)
            image_json.push_back(item->to_json());

        j = json{{"name", p.name, "images", image_json}};
    }

    void to_json(json& j, const Root& p) {
        spdlog::debug("To json root", to_string(j));
        j = json{{"groups", p.presets}, {"curr", p.curr}};
    }

    void from_json(const json& j, Root& p) {
        spdlog::debug("from json root", to_string(j));
        j.at("curr").get_to(p.curr);
        j.at("groups").get_to(p.presets);
    }

    void from_json(const json& j, Preset& p) {
        spdlog::debug("from json group", to_string(j));
        j.at("name").get_to(p.name);

        vector<json> image_json;
        j.at("images").get_to(image_json);

        vector<ImageTypes::General*> images;
        images.reserve(image_json.size());

        for (const auto &item: image_json)
            images.push_back(ImageTypes::General::from_json(item));

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