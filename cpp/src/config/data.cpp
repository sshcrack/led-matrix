//
// Created by hendrik on 2/2/24.
//

#include "data.h"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

namespace ConfigData {
    void to_json(json& j, const ImageTypes::General*& p)  {
        j = p->to_json();
    }

    void to_json(json& j, const Groups& p)  {
        vector<json> image_json;

        image_json.reserve(p.images.size());
        for (const auto &item: p.images)
            image_json.push_back(item->to_json());

        j = json{{"name", p.name, "images", image_json}};
    }

    void to_json(json& j, const Root& p) {
        j = json{{"groups", p.groups}, {"curr", p.curr}};
    }

    void from_json(const json& j, Root& p) {
        j.at("curr").get_to(p.curr);
        j.at("groups").get_to(p.groups);
    }

    void from_json(const json& j, Groups& p) {
        j.at("name").get_to(p.name);

        vector<json> image_json;
        j.at("images").get_to(image_json);

        vector<ImageTypes::General*> images;
        images.reserve(image_json.size());

        for (const auto &item: image_json)
            images.push_back(ImageTypes::General::from_json(item));

        p.images = images;
    }

    void from_json(const json& j, ImageTypes::General*& p) {
        p = ImageTypes::General::from_json(j);
    }
}